#pragma once
#include <memory>
#include <string>
#include <vector>
#include <glog/logging.h>
#include <wangle/concurrent/CPUThreadPoolExecutor.h>
#include <wangle/concurrent/FutureExecutor.h>
#include <folly/futures/Future.h>
#include <folly/futures/Try.h>
#include <folly/Optional.h>

#include "declarations.h"

#include "util/util.h"

namespace relevanced {
namespace persistence {

class SyncPersistenceIf {
 public:
  virtual bool
    doesDocumentExist(const std::string &id) = 0;

  virtual folly::Try<bool>
    saveDocument(std::shared_ptr<models::ProcessedDocument> doc) = 0;

  virtual folly::Try<bool>
    saveNewDocument(std::shared_ptr<models::ProcessedDocument> doc) = 0;

  virtual folly::Try<bool>
    deleteDocument(const std::string &id) = 0;

  virtual std::vector<std::string>
    listAllDocuments() = 0;

  virtual std::vector<std::string>
    listDocumentRangeFromId(const std::string &id, size_t count) = 0;

  virtual std::vector<std::string>
    listDocumentRangeFromOffset(size_t offset, size_t count) = 0;

  virtual folly::Try<std::shared_ptr<models::ProcessedDocument>>
    loadDocument(const std::string &) = 0;

  virtual bool
    doesCentroidExist(const std::string &id) = 0;

  virtual folly::Try<bool>
    createNewCentroid(const std::string &id) = 0;

  virtual folly::Try<bool>
    deleteCentroid(const std::string &id) = 0;

  virtual folly::Try<bool>
    saveCentroid(const std::string &id, std::shared_ptr<models::Centroid>) = 0;

  virtual folly::Try<std::shared_ptr<models::Centroid>>
    loadCentroid(const std::string &id) = 0;

  virtual folly::Optional<util::UniquePointer<models::Centroid>>
    loadCentroidUniqueOption(const std::string &id) = 0;

  virtual std::vector<std::string>
    listAllCentroids() = 0;

  virtual std::vector<std::string>
    listCentroidRangeFromOffset(size_t offset, size_t limit) = 0;

  virtual std::vector<std::string>
    listCentroidRangeFromId(const std::string &startingCentroidId, size_t limit) = 0;

  virtual folly::Try<bool>
    addDocumentToCentroid(const std::string&, const std::string&) = 0;

  virtual folly::Try<bool>
    removeDocumentFromCentroid(const std::string&, const std::string&) = 0;

  virtual folly::Try<bool>
    doesCentroidHaveDocument(const std::string&, const std::string&) = 0;

  virtual folly::Try<std::vector<std::string>>
    listAllDocumentsForCentroid(const std::string &) = 0;

  virtual folly::Optional<std::vector<std::string>>
    listAllDocumentsForCentroidOption(const std::string &) = 0;

  virtual folly::Try<std::vector<std::string>>
    listCentroidDocumentRangeFromOffset(
      const std::string &,
      size_t offset,
      size_t limit
    ) = 0;

  virtual folly::Optional<std::vector<std::string>>
    listCentroidDocumentRangeFromOffsetOption(
      const std::string &,
      size_t offset,
      size_t limit
    ) = 0;

  virtual folly::Try<std::vector<std::string>>
    listCentroidDocumentRangeFromDocumentId(
      const std::string &,
      const std::string &,
      size_t limit
    ) = 0;

  virtual folly::Optional<std::vector<std::string>>
    listCentroidDocumentRangeFromDocumentIdOption(
      const std::string &,
      const std::string &,
      size_t limit
    ) = 0;

  virtual folly::Optional<std::string>
    getCentroidMetadata(const std::string&, const std::string&) = 0;

  virtual folly::Try<bool>
    setCentroidMetadata(const std::string&, const std::string&, std::string) = 0;

  virtual void debugEraseAllData() = 0;

  virtual std::vector<std::string>
    listUnusedDocuments(size_t count) = 0;

  virtual ~SyncPersistenceIf() = default;
};

class SyncPersistence : public SyncPersistenceIf {
 protected:
  std::shared_ptr<util::ClockIf> clock_;
  util::UniquePointer<RockHandleIf> rockHandle_;

  std::vector<std::string>
    listAllDocumentsForCentroidRaw(const std::string &);

  std::vector<std::string>
    listCentroidDocumentRangeFromOffsetRaw(
      const std::string &id,
      size_t offset,
      size_t count
    );

  std::vector<std::string>
    listCentroidDocumentRangeFromDocumentIdRaw(
      const std::string &,
      const std::string &,
      size_t
    );

  bool isDocumentInAnyCentroid(const std::string&);

  std::vector<std::string>
    listDocumentCentroids(const std::string&);

  folly::Optional<int64_t>
    getDocumentCreatedTime(const std::string&);

  void setDocumentCreatedTime(const std::string&, int64_t);

  static std::string
    getCentroidsPrefix();

  static std::string
    getDocumentsPrefix();

  static std::string
    getCentroidKey(const std::string&);

  static std::string
    getDocumentKey(const std::string&);

  static std::string
    getDocumentCentroidsPrefix(const std::string&);

  static std::string
    getDocumentCentroidKey(const std::string&, const std::string&);

  static std::string
    getCentroidDocumentPrefix(const std::string&);

  static std::string
    getCentroidDocumentKey(const std::string&, const std::string&);

  static std::string
    getCentroidMetadataKey(const std::string&, const std::string&);

  static std::string
    getDocumentMetadataKey(const std::string&, const std::string&);

 public:
  SyncPersistence(
    std::shared_ptr<util::ClockIf>,
    util::UniquePointer<RockHandleIf>
  );

  SyncPersistence(SyncPersistence const &) = delete;
  void operator=(SyncPersistence const &) = delete;

  bool doesDocumentExist(const std::string &id) override;

  folly::Try<bool>
    saveNewDocument(models::ProcessedDocument *doc);

  folly::Try<bool>
    saveNewDocument(
      std::shared_ptr<models::ProcessedDocument> doc
    ) override;

  folly::Try<bool>
    saveDocument(models::ProcessedDocument *doc);

  folly::Try<bool>
    saveDocument(
      std::shared_ptr<models::ProcessedDocument> doc
    ) override;

  folly::Try<bool>
    deleteDocument(const std::string &id) override;

  std::vector<std::string>
    listAllDocuments() override;

  std::vector<std::string>
    listUnusedDocuments(size_t count) override;

  size_t deleteOldUnusedDocuments(int64_t minAge, size_t count);

  std::vector<std::string>
    listDocumentRangeFromId(const std::string &id, size_t count) override;

  std::vector<std::string>
    listDocumentRangeFromOffset(size_t offset, size_t count) override;

  folly::Try<std::shared_ptr<models::ProcessedDocument>>
    loadDocument(const std::string&) override;

  bool doesCentroidExist(const std::string &id) override;

  folly::Try<bool>
    createNewCentroid(const std::string &id) override;

  folly::Try<bool>
    deleteCentroid(const std::string &id) override;

  folly::Try<bool>
    saveCentroid(const std::string &id, models::Centroid *);

  folly::Try<bool>
    saveCentroid(
      const std::string &id,
      std::shared_ptr<models::Centroid>
    ) override;

  folly::Try<std::shared_ptr<models::Centroid>>
    loadCentroid(const std::string &id) override;

  folly::Optional<util::UniquePointer<models::Centroid>>
    loadCentroidUniqueOption(const std::string &id) override;

  std::vector<std::string>
    listAllCentroids() override;

  std::vector<std::string>
    listCentroidRangeFromOffset(
      size_t offset,
      size_t limit
    ) override;

  std::vector<std::string>
    listCentroidRangeFromId(
      const std::string &startingCentroidId,
      size_t limit
    ) override;

  folly::Try<bool>
    addDocumentToCentroid(
      const std::string &,
      const std::string &
    ) override;

  folly::Try<bool>
    removeDocumentFromCentroid(
      const std::string &,
      const std::string &
    ) override;

  folly::Try<bool>
    doesCentroidHaveDocument(
      const std::string &,
      const std::string &
    ) override;

  folly::Try<std::vector<std::string>>
    listAllDocumentsForCentroid(const std::string &) override;

  folly::Optional<std::vector<std::string>>
    listAllDocumentsForCentroidOption(const std::string &) override;

  folly::Try<std::vector<std::string>>
    listCentroidDocumentRangeFromOffset(
      const std::string &,
      size_t offset,
      size_t limit
    ) override;

  folly::Optional<std::vector<std::string>>
    listCentroidDocumentRangeFromOffsetOption(
      const std::string &,
      size_t offset,
      size_t limit
    ) override;

  folly::Try<std::vector<std::string>>
    listCentroidDocumentRangeFromDocumentId(
      const std::string &,
      const std::string &,
      size_t limit
    ) override;

  folly::Optional<std::vector<std::string>>
    listCentroidDocumentRangeFromDocumentIdOption(
      const std::string &,
      const std::string &,
      size_t limit
    ) override;

  folly::Optional<std::string>
    getCentroidMetadata(
      const std::string &,
      const std::string &
    ) override;

  folly::Try<bool>
    setCentroidMetadata(
      const std::string &,
      const std::string &,
      std::string
    ) override;

  void debugEraseAllData() override;
};

} // persistence
} // relevanced
