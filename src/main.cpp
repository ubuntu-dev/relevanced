#include <string>
#include <memory>
#include <chrono>

#include <glog/logging.h>
#include "stopwords/StopwordFilter.h"
#include "tokenizer/Tokenizer.h"
#include "stemmer/StemmerIf.h"
#include "stemmer/PorterStemmer.h"
#include "DocumentProcessor.h"
#include "persistence/PersistenceService.h"
#include "persistence/RockHandle.h"
#include "persistence/ColonPrefixedRockHandle.h"
#include "ProcessedDocument.h"
#include "serialization/serializers.h"
#include "persistence/CollectionDB.h"
#include "persistence/CollectionDBHandle.h"
#include "RelevanceServer.h"
#include "ThriftRelevanceServer.h"
#include "persistence/DocumentDB.h"
#include "persistence/DocumentDBHandle.h"
#include "persistence/CentroidDB.h"
#include "persistence/CentroidDBHandle.h"
#include "util.h"
#include "RelevanceScoreWorker.h"
#include <wangle/concurrent/CPUThreadPoolExecutor.h>
#include <wangle/concurrent/FutureExecutor.h>
#include <folly/futures/Future.h>
#include <thrift/lib/cpp2/server/ThriftServer.h>
#include <thrift/lib/cpp2/async/AsyncProcessor.h>
#include <rocksdb/slice.h>

using namespace std;
using namespace folly;
using namespace persistence;
using stemmer::StemmerIf;
using stemmer::PorterStemmer;
using stopwords::StopwordFilter;
using stopwords::StopwordFilterIf;
using tokenizer::TokenizerIf;
using tokenizer::Tokenizer;
using wangle::CPUThreadPoolExecutor;
using wangle::FutureExecutor;
using util::UniquePointer;

shared_ptr<PersistenceServiceIf> getPersistence() {

  auto threadPool = std::make_shared<FutureExecutor<CPUThreadPoolExecutor>>(4);

  UniquePointer<RockHandleIf> centroidRock(
    (RockHandleIf*) new RockHandle("data/centroids")
  );
  UniquePointer<CentroidDBHandleIf> centroidDbHandle(
    (CentroidDBHandleIf*) new CentroidDBHandle(std::move(centroidRock))
  );
  shared_ptr<CentroidDBIf> centroidDb(
    (CentroidDBIf*) new CentroidDB(
      std::move(centroidDbHandle),
      make_shared<FutureExecutor<CPUThreadPoolExecutor>>(1)
    )
  );

  UniquePointer<RockHandleIf> docRock(
    (RockHandleIf*) new RockHandle("data/documents")
  );
  UniquePointer<DocumentDBHandleIf> docDbHandle(
    (DocumentDBHandleIf*) new DocumentDBHandle(std::move(docRock))
  );
  shared_ptr<DocumentDBIf> docDb(
    (DocumentDBIf*) new DocumentDB(
      std::move(docDbHandle),
      make_shared<FutureExecutor<CPUThreadPoolExecutor>>(4)
    )
  );

  UniquePointer<RockHandleIf> collectionListRock(
    (RockHandleIf*) new RockHandle("data/collections_rock")
  );
  UniquePointer<ColonPrefixedRockHandle> collectionDocumentsRock(
    new ColonPrefixedRockHandle("data/collection_docs_rock")
  );
  UniquePointer<CollectionDBHandleIf> collDbHandle(
    (CollectionDBHandleIf*) new CollectionDBHandle(
      std::move(collectionDocumentsRock), std::move(collectionListRock)
    )
  );
  collDbHandle->ensureTables();
  shared_ptr<CollectionDBIf> collDb(
    (CollectionDBIf*) new CollectionDB(
      std::move(collDbHandle),
      make_shared<FutureExecutor<CPUThreadPoolExecutor>>(4)
    )
  );
  collDb->initialize();

  shared_ptr<PersistenceServiceIf> persistence(
    (PersistenceServiceIf*) new PersistenceService(
      centroidDb, docDb, collDb
    )
  );
  return std::move(persistence);
}

shared_ptr<CentroidManager> getCentroidManager(shared_ptr<PersistenceServiceIf> persistence) {
  auto threadPool = make_shared<FutureExecutor<CPUThreadPoolExecutor>>(2);
  UniquePointer<CentroidUpdater> updater(
    new CentroidUpdater(
      persistence,
      threadPool
    )
  );
  return make_shared<CentroidManager>(
    std::move(updater), persistence
  );
}

int main() {
  LOG(INFO) << "start";
  thread t1([](){
    shared_ptr<TokenizerIf> tokenizer(
      (TokenizerIf*) new Tokenizer
    );
    shared_ptr<StemmerIf> stemmer(
      (StemmerIf*) new PorterStemmer
    );
    shared_ptr<StopwordFilterIf> stopwordFilter(
      (StopwordFilterIf*) new StopwordFilter
    );
    auto documentProcessor = make_shared<DocumentProcessor>(
      stemmer,
      tokenizer,
      stopwordFilter
    );
    auto persistence = getPersistence();
    auto centroidManager = getCentroidManager(persistence);

    auto relevanceWorker = make_shared<RelevanceScoreWorker>(
      persistence, centroidManager, documentProcessor
    );
    relevanceWorker->initialize();
    auto relServer = make_shared<RelevanceServer>(
      relevanceWorker, documentProcessor, persistence
    );
    auto service = make_shared<ThriftRelevanceServer>(relServer);
    bool allowInsecureLoopback = true;
    string saslPolicy = "";
    auto server = new apache::thrift::ThriftServer(
      saslPolicy, allowInsecureLoopback
    );
    server->setInterface(service);
    server->setTaskExpireTime(chrono::milliseconds(60000));
    auto port = 8097;
    server->setPort(port);
    LOG(INFO) << "listening on: " << port;
    server->serve();
  });
  t1.join();
  LOG(INFO) << "end";
}


// int main() {
//   shared_ptr<TokenizerIf> tokenizer(
//     (TokenizerIf*) new Tokenizer
//   );
//   shared_ptr<StopwordFilterIf> stopwordFilter(
//     (StopwordFilterIf*) new StopwordFilter
//   );
//   shared_ptr<StemmerIf> stemmer(
//     (StemmerIf*) new PorterStemmer
//   );
//   DocumentProcessor proc(stemmer, tokenizer, stopwordFilter);

//   Document doc1("doc-1", "a fish is a type of animal which is different from a cat.");
//   Document doc2("doc-2", "a fish is also something that lives with other fish in an aquarium.");
//   ProcessedDocument d1 = proc.process(doc1);
//   ProcessedDocument d2 = proc.process(doc2);
// }