#include <string>
#include <memory>
#include <folly/futures/Promise.h>
#include <folly/futures/Future.h>
#include <folly/futures/helpers.h>
#include <folly/futures/Try.h>
#include <folly/Optional.h>
#include <folly/Format.h>
#include <folly/ExceptionWrapper.h>

#include <glog/logging.h>

#include "centroid_update_worker/CentroidUpdateWorker.h"
#include "document_processing_worker/DocumentProcessor.h"
#include "document_processing_worker/DocumentProcessingWorker.h"
#include "models/Document.h"
#include "models/Centroid.h"
#include "models/ProcessedDocument.h"
#include "gen-cpp2/RelevancedProtocol_types.h"
#include "persistence/Persistence.h"
#include "persistence/CentroidMetadataDb.h"
#include "release_metadata/release_metadata.h"
#include "serialization/serializers.h"
#include "server/RelevanceServer.h"
#include "similarity_score_worker/SimilarityScoreWorker.h"
#include "util/util.h"
#include "util/Clock.h"

namespace relevanced {
namespace server {

using namespace std;
using namespace folly;
using thrift_protocol::EDocumentAlreadyExists;
using thrift_protocol::ECentroidDoesNotExist;

using similarity_score_worker::SimilarityScoreWorkerIf;
using centroid_update_worker::CentroidUpdateWorkerIf;
using document_processing_worker::DocumentProcessingWorkerIf;
using models::Document;
using models::ProcessedDocument;
using models::Centroid;

using util::UniquePointer;

RelevanceServer::RelevanceServer(
    shared_ptr<persistence::PersistenceIf> persistenceSv,
    shared_ptr<persistence::CentroidMetadataDbIf> metadataDb,
    shared_ptr<util::ClockIf> clock,
    shared_ptr<SimilarityScoreWorkerIf> scoreWorker,
    shared_ptr<DocumentProcessingWorkerIf> docProcessor,
    shared_ptr<CentroidUpdateWorkerIf> centroidUpdater)
    : persistence_(persistenceSv),
      centroidMetadataDb_(metadataDb),
      clock_(clock),
      scoreWorker_(scoreWorker),
      processingWorker_(docProcessor),
      centroidUpdateWorker_(centroidUpdater) {}


void RelevanceServer::ping() {}


void RelevanceServer::initialize() {
  centroidUpdateWorker_->initialize();
  scoreWorker_->initialize();
  centroidUpdateWorker_->onUpdate(
    [this](const string &id) {
      scoreWorker_->reloadCentroid(id);
    });
}


Future<Try<double>> RelevanceServer::getDocumentSimilarity(
    unique_ptr<string> centroidId, unique_ptr<string> docId) {
  string cId = *centroidId;
  return persistence_->loadDocument(*docId)
    .then([this, cId](
        Try<shared_ptr<ProcessedDocument>> doc) {
      if (doc.hasException()) {
        return makeFuture<Try<double>>(
          Try<double>(doc.exception())
        );
      }
      return scoreWorker_->getDocumentSimilarity(
        cId, doc.value()
      );
    });
}


Future<Try<double>> RelevanceServer::getCentroidSimilarity(
    unique_ptr<string> centroid1Id, unique_ptr<string> centroid2Id) {
  return scoreWorker_->getCentroidSimilarity(
    *centroid1Id,
    *centroid2Id
  );
}


Future<Try<unique_ptr<map<string, double>>>>
RelevanceServer::internalMultiGetDocumentSimilarity(
    shared_ptr<vector<string>> centroidIds,
    shared_ptr<ProcessedDocument> doc) {
  vector<Future<Try<double>>> scores;
  for (size_t i = 0; i < centroidIds->size(); i++) {
    scores.push_back(
      scoreWorker_->getDocumentSimilarity(
        centroidIds->at(i), doc
      )
    );
  }
  return collect(scores)
    .then([this, centroidIds](Try<vector<Try<double>>> scores) {
      if (scores.hasException()) {
        return Try<unique_ptr<map<string, double>>>(
          scores.exception()
        );
      }
      auto scoreVals = scores.value();
      auto response = folly::make_unique<map<string, double>>();
      for (size_t i = 0; i < centroidIds->size(); i++) {
        auto currentScoreVal = scoreVals.at(i);
        if (currentScoreVal.hasException()) {
          return Try<unique_ptr<map<string, double>>>(
            currentScoreVal.exception()
          );
        }
        response->insert(make_pair(
          centroidIds->at(i), currentScoreVal.value()
        ));
      }
      return Try<unique_ptr<map<string, double>>>(
        std::move(response)
      );
    });
}


Future<Try<unique_ptr<map<string, double>>>>
RelevanceServer::multiGetTextSimilarity(
    unique_ptr<vector<string>> centroidIds, unique_ptr<string> text) {
  auto doc = std::make_shared<Document>("no-id", *text);
  auto cIds = std::make_shared<vector<string>>(*centroidIds);
  return processingWorker_->processNew(doc)
    .then([this, cIds](shared_ptr<ProcessedDocument> processed) {
      return internalMultiGetDocumentSimilarity(cIds, processed);
    });
}


Future<Try<unique_ptr<map<string, double>>>>
RelevanceServer::multiGetDocumentSimilarity(
    unique_ptr<vector<string>> centroidIds, unique_ptr<string> docId) {
  auto cIds = std::make_shared<vector<string>>(*centroidIds);
  return persistence_->loadDocument(*docId)
    .then([this, cIds](Try<shared_ptr<ProcessedDocument>> doc)  {
      if (doc.hasException()) {
        return makeFuture<Try<unique_ptr<map<string, double>>>>(
          doc.exception()
        );
      }
      return this->internalMultiGetDocumentSimilarity(
        cIds, doc.value()
      );
    });
}


Future<Try<double>> RelevanceServer::getTextSimilarity(
    unique_ptr<string> centroidId, unique_ptr<string> text) {
  auto doc = std::make_shared<Document>("no-id", *text);
  auto cId = *centroidId;
  return processingWorker_->processNew(doc)
    .then([this, cId](shared_ptr<ProcessedDocument> processed) {
      return scoreWorker_->getDocumentSimilarity(cId, processed);
    });
}


Future<Try<unique_ptr<string>>> RelevanceServer::createDocument(
    unique_ptr<string> text) {
  return internalCreateDocumentWithID(util::getUuid(), *text);
}


Future<Try<unique_ptr<string>>> RelevanceServer::internalCreateDocumentWithID(
    string id, string text) {
  auto doc = std::make_shared<Document>(id, text);
  return processingWorker_->processNew(doc)
    .then([this, id](shared_ptr<ProcessedDocument> processed) {
      return persistence_->saveNewDocument(processed)
        .then([this, id](Try<bool> result) {
          if (result.hasException()) {
            return Try<unique_ptr<string>>(
              result.exception()
            );
          }
          return Try<unique_ptr<string>>(
            folly::make_unique<string>(id)
          );
        });
    });
}


Future<Try<unique_ptr<string>>> RelevanceServer::createDocumentWithID(
    unique_ptr<string> id, unique_ptr<string> text) {
  return internalCreateDocumentWithID(*id, *text);
}


Future<Try<bool>> RelevanceServer::deleteDocument(unique_ptr<string> id) {
  return persistence_->deleteDocument(*id);
}


Future<Try<unique_ptr<string>>> RelevanceServer::getDocument(
    unique_ptr<string> id) {
  return persistence_->loadDocument(*id)
    .then([](Try<shared_ptr<ProcessedDocument>> doc) {
      if (doc.hasException()) {
        return Try<unique_ptr<string>>(doc.exception());
      }
      string serialized;
      serialization::jsonSerialize(serialized, doc.value().get());
      auto uniq = folly::make_unique<string>(serialized);
      return Try<unique_ptr<string>>(std::move(uniq));
    });
}


Future<Try<bool>> RelevanceServer::createCentroid(
    unique_ptr<string> centroidId) {
  auto id = *centroidId;
  return persistence_->createNewCentroid(id);
}


Future<Try<bool>> RelevanceServer::deleteCentroid(
    unique_ptr<string> centroidId) {
  auto cId = *centroidId;
  return persistence_->deleteCentroid(cId);
}


Future<Try<unique_ptr<vector<string>>>>
RelevanceServer::listAllDocumentsForCentroid(
    unique_ptr<string> centroidId) {
  return persistence_->listAllDocumentsForCentroid(*centroidId)
    .then([](Try<vector<string>> docIds) {
      if (docIds.hasException()) {
        return Try<unique_ptr<vector<string>>>(
          docIds.exception()
        );
      }
      vector<string> docs = docIds.value();
      return Try<unique_ptr<vector<string>>>(
        folly::make_unique<vector<string>>(std::move(docs))
      );
    });
}

Future<Try<unique_ptr<vector<string>>>>
RelevanceServer::listCentroidDocumentRange(
    unique_ptr<string> centroidId, size_t offset, size_t count) {
  return persistence_->listCentroidDocumentRangeFromOffset(
    *centroidId, offset, count
  ).then([](Try<vector<string>> docIds) {
      if (docIds.hasException()) {
        return Try<unique_ptr<vector<string>>>(docIds.exception());
      }
      return Try<unique_ptr<vector<string>>>(
        folly::make_unique<vector<string>>(std::move(docIds.value()))
      );
    });
}


Future<Try<unique_ptr<vector<string>>>>
RelevanceServer::listCentroidDocumentRangeFromID(
    unique_ptr<string> centroidId,
    unique_ptr<string> documentId,
    size_t count) {
  return persistence_->listCentroidDocumentRangeFromDocumentId(
    *centroidId, *documentId, count
  ).then([](Try<vector<string>> docIds) {
      if (docIds.hasException()) {
        return Try<unique_ptr<vector<string>>>(docIds.exception());
      }
      return Try<unique_ptr<vector<string>>>(
        folly::make_unique<vector<string>>(std::move(docIds.value()))
      );
    });
}


Future<Try<bool>> RelevanceServer::addDocumentToCentroid(
    unique_ptr<string> centroidId, unique_ptr<string> docId) {
  auto cId = *centroidId;
  return persistence_->addDocumentToCentroid(cId, *docId)
    .then([this, cId](Try<bool> result) {
      if (!result.hasException()) {
        centroidMetadataDb_->setLastDocumentChangeTimestamp(
          cId, clock_->getEpochTime()
        );
        centroidUpdateWorker_->triggerUpdate(cId);
      }
      return result;
    });
}


Future<Try<bool>> RelevanceServer::removeDocumentFromCentroid(
    unique_ptr<string> centroidId, unique_ptr<string> docId) {
  auto cId = *centroidId;
  return persistence_->removeDocumentFromCentroid(cId, *docId)
    .then([this, cId](Try<bool> result) {
      if (!result.hasException()) {
        centroidMetadataDb_->setLastDocumentChangeTimestamp(
          cId, clock_->getEpochTime()
        );
        centroidUpdateWorker_->triggerUpdate(cId);
      }
      return result;
    });
}


Future<Try<bool>> RelevanceServer::joinCentroid(
    unique_ptr<string> centroidId) {
  auto cId = *centroidId;
  return centroidMetadataDb_->isCentroidUpToDate(cId)
    .then([this, cId](Try<bool> isUpToDate) {
      if (isUpToDate.hasException()) {
        Try<bool> result(isUpToDate.exception());
        return makeFuture(result);
      }
      if (isUpToDate.value()) {
        bool recomputed = false;
        Try<bool> result(recomputed);
        return makeFuture(result);
      }
      return centroidUpdateWorker_->joinUpdate(cId)
        .then([this, cId](Try<string> result) {
          if (result.hasException()) {
            Try<bool> toReturn(result.exception());
            return makeFuture(toReturn);
          }
          return scoreWorker_->reloadCentroid(cId).then([](){
            bool recomputed = true;
            return Try<bool>(recomputed);
          });
        });
    });
}


Future<unique_ptr<vector<string>>> RelevanceServer::listAllCentroids() {
  return persistence_->listAllCentroids()
    .then([](vector<string> centroidIds) {
      return std::move(folly::make_unique<vector<string>>(centroidIds));
    });
}


Future<unique_ptr<vector<string>>> RelevanceServer::listCentroidRange(
    size_t offset, size_t count) {
  return persistence_->listCentroidRangeFromOffset(offset, count)
    .then([](vector<string> centroidIds) {
      return std::move(folly::make_unique<vector<string>>(
        std::move(centroidIds)
      ));
    });
}


Future<unique_ptr<vector<string>>> RelevanceServer::listCentroidRangeFromID(
    unique_ptr<string> centroidId, size_t count) {
  return persistence_->listCentroidRangeFromId(*centroidId, count)
    .then([](vector<string> centroidIds) {
      return std::move(folly::make_unique<vector<string>>(
        std::move(centroidIds)
      ));
    });
}


Future<unique_ptr<vector<string>>> RelevanceServer::listAllDocuments() {
  return persistence_->listAllDocuments()
    .then([](vector<string> docIds) {
      return std::move(
        folly::make_unique<vector<string>>(docIds)
      );
    });
}


Future<unique_ptr<vector<string>>> RelevanceServer::listDocumentRange(
    size_t offset, size_t count) {
  return persistence_->listDocumentRangeFromOffset(offset, count)
    .then([](vector<string> docIds) {
      return std::move(folly::make_unique<vector<string>>(
        std::move(docIds)
      ));
    });
}


Future<unique_ptr<vector<string>>> RelevanceServer::listDocumentRangeFromID(
    unique_ptr<string> docId, size_t count) {
  return persistence_->listDocumentRangeFromId(*docId, count)
    .then([](vector<string> docIds) {
      return std::move(folly::make_unique<vector<string>>(
        std::move(docIds)
      ));
    });
}

Future<unique_ptr<map<string, string>>> RelevanceServer::getServerMetadata() {
  string revision = release_metadata::getGitRevisionSha();
  string version = release_metadata::getGitVersion();
  auto metadata = folly::make_unique<map<string, string>>();
  metadata->insert(make_pair(
    "relevanced_git_revision",
    revision
  ));
  metadata->insert(make_pair(
    "relevanced_version",
    version
  ));
  metadata->insert(make_pair(
    "relevanced_utc_build_timestamp",
    release_metadata::getUtcBuildTimestamp()
  ));
  return makeFuture(std::move(metadata));
}

Future<folly::Unit> RelevanceServer::debugEraseAllData() {
  return persistence_->debugEraseAllData();
}

Future<Try<shared_ptr<Centroid>>> RelevanceServer::debugGetFullCentroid(
    unique_ptr<string> centroidId) {
  return persistence_->loadCentroid(*centroidId);
}

Future<Try<shared_ptr<ProcessedDocument>>> RelevanceServer::debugGetFullProcessedDocument(
    unique_ptr<string> documentId) {
  return persistence_->loadDocument(*documentId);
}



} // server
} // relevanced