#pragma once

#include <memory>
#include <folly/futures/Future.h>
#include <wangle/concurrent/CPUThreadPoolExecutor.h>
#include <wangle/concurrent/FutureExecutor.h>

#include "declarations.h"

namespace relevanced {
namespace document_processing_worker {

class DocumentProcessingWorkerIf {
 public:
  virtual folly::Future<std::shared_ptr<models::ProcessedDocument>>
    processNew(std::shared_ptr<models::Document>) = 0;

  virtual folly::Future<std::shared_ptr<models::ProcessedDocument>>
    processNewWithoutHash(std::shared_ptr<models::Document>) = 0;
};


class DocumentProcessingWorker : public DocumentProcessingWorkerIf {
 protected:
  std::shared_ptr<DocumentProcessorIf> processor_;
  std::shared_ptr<util::Sha1HasherIf> hasher_;
  std::shared_ptr<wangle::FutureExecutor<wangle::CPUThreadPoolExecutor>>
      threadPool_;

 public:
  DocumentProcessingWorker(
    std::shared_ptr<DocumentProcessorIf>,
    std::shared_ptr<util::Sha1HasherIf>,
    std::shared_ptr<wangle::FutureExecutor<wangle::CPUThreadPoolExecutor>>
  );

  folly::Future<std::shared_ptr<models::ProcessedDocument>>
    processNew(std::shared_ptr<models::Document>) override;

  folly::Future<std::shared_ptr<models::ProcessedDocument>>
    processNewWithoutHash(std::shared_ptr<models::Document>) override;
};

} // document_processing_worker
} // relevanced