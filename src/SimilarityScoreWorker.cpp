#include <memory>
#include <wangle/concurrent/CPUThreadPoolExecutor.h>
#include <wangle/concurrent/FutureExecutor.h>
#include <folly/futures/Future.h>
#include <folly/futures/helpers.h>

#include <folly/Optional.h>
#include <folly/Synchronized.h>
#include <vector>
#include <cassert>

#include "persistence/Persistence.h"
#include "CentroidUpdateWorker.h"
#include "Centroid.h"
#include "DocumentProcessor.h"
#include "ProcessedDocument.h"
#include "SimilarityScoreWorker.h"
#include "util.h"

using namespace wangle;
using namespace folly;
using namespace std;

SimilarityScoreWorker::SimilarityScoreWorker(
  shared_ptr<persistence::PersistenceIf> persistence,
  shared_ptr<FutureExecutor<CPUThreadPoolExecutor>> threadPool
): persistence_(persistence), threadPool_(threadPool) {}

// run synchronously on startup
void SimilarityScoreWorker::initialize() {
  LOG(INFO) << "SimilarityScoreWorker initializing...";
  auto centroidIds = persistence_->listAllCentroids().get();
  SYNCHRONIZED(centroids_) {
    for (auto &id: centroidIds) {
      LOG(INFO) << format("loading centroid '{}' ...", id);;
      auto centroid = persistence_->loadCentroidOption(id).get();
      if (centroid.hasValue()) {
        centroids_[id] = centroid.value();
      } else {
        LOG(INFO) << format("centroid '{}' doesn't seem to exist...", id);
      }
    }
  }
  LOG(INFO) << "SimilarityScoreWorker initialized.";
}

Future<bool> SimilarityScoreWorker::reloadCentroid(string id) {
  return persistence_->loadCentroidOption(id).then([id, this](Optional<shared_ptr<Centroid>> centroid) {
    if (!centroid.hasValue()) {
      LOG(INFO) << format("tried to reload null centroid '{}'", id);
      return false;
    }
    SYNCHRONIZED(centroids_) {
      centroids_[id] = centroid.value();
    }
    LOG(INFO) << format("inserted reloaded centroid '{}'", id);
    return true;
  });
}

Optional<shared_ptr<Centroid>> SimilarityScoreWorker::getLoadedCentroid_(const string &id) {
  Optional<shared_ptr<Centroid>> center;
  SYNCHRONIZED(centroids_) {
    auto elem = centroids_.find(id);
    if (elem != centroids_.end()) {
      center.assign(elem->second);
    }
  }
  return center;
}

void SimilarityScoreWorker::setLoadedCentroid_(const string &id, shared_ptr<Centroid> centroid) {
  SYNCHRONIZED(centroids_) {
    centroids_[id] = centroid;
  }
}

Future<double> SimilarityScoreWorker::getDocumentSimilarity(string centroidId, ProcessedDocument *doc) {
  return threadPool_->addFuture([this, centroidId, doc](){
    auto centroid = getLoadedCentroid_(centroidId);
    if (!centroid.hasValue()) {
      LOG(INFO) << "relevance request against null centroid: " << centroidId;
      return 0.0;
    }
    return centroid.value()->score(doc);
  });
}

Future<double> SimilarityScoreWorker::getDocumentSimilarity(string centroidId, shared_ptr<ProcessedDocument> doc) {
  return getDocumentSimilarity(centroidId, doc.get());
}