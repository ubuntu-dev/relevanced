#include <string>
#include <memory>
#include <vector>

#include <wangle/concurrent/CPUThreadPoolExecutor.h>
#include <wangle/concurrent/FutureExecutor.h>
#include <folly/futures/Future.h>
#include <folly/futures/helpers.h>

#include "DocumentDB.h"
#include "DocumentDBCache.h"
#include "DocumentDBHandle.h"
#include "RockHandle.h"
#include "ProcessedDocument.h"
#include "util.h"
using namespace std;
using namespace folly;
using namespace wangle;
using util::UniquePointer;
namespace persistence {

DocumentDB::DocumentDB(UniquePointer<DocumentDBHandleIf> dbHandle, shared_ptr<FutureExecutor<CPUThreadPoolExecutor>> threadPool)
  : dbHandle_(std::move(dbHandle)), threadPool_(threadPool){
    dbCache_ = make_shared<DocumentDBCache>();
}

Future<bool> DocumentDB::doesDocumentExist(const string &docId) {
  if (dbCache_->exists(docId)) {
    return makeFuture(true);
  }
  return threadPool_->addFuture([this, docId](){
    bool exists = dbHandle_->doesDocumentExist(docId);
    if (exists) {
      dbCache_->add(docId);
    }
    return exists;
  });
}

Future<bool> DocumentDB::deleteDocument(const string &docId) {
  dbCache_->remove(docId);
  threadPool_->addFuture([this, docId](){
    dbHandle_->deleteDocument(docId);
  });
  return makeFuture(true);
}

Future<bool> DocumentDB::saveDocument(ProcessedDocument *doc) {
  dbCache_->add(doc->id);
  threadPool_->addFuture([this, doc](){
    dbHandle_->saveDocument(doc);
  });
  return makeFuture(true);
}

Future<ProcessedDocument*> DocumentDB::loadDocument(const string &docId) {
  return threadPool_->addFuture([this, docId](){
    return dbHandle_->loadDocument(docId);
  });
}

} // persistence
