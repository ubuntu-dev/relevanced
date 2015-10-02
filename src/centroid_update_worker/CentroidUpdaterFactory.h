#pragma once
#include <vector>
#include <memory>
#include <string>
#include "persistence/Persistence.h"
#include "centroid_update_worker/CentroidUpdater.h"
#include "util/util.h"

namespace relevanced {
namespace centroid_update_worker {

class CentroidUpdaterFactoryIf {
public:
  virtual std::shared_ptr<CentroidUpdaterIf> makeForCentroidId(const std::string&) = 0;
  virtual ~CentroidUpdaterFactoryIf() = default;
};

class CentroidUpdaterFactory: public CentroidUpdaterFactoryIf {
protected:
  std::shared_ptr<persistence::PersistenceIf> persistence_;
public:
  CentroidUpdaterFactory(
    std::shared_ptr<persistence::PersistenceIf>
  );
  std::shared_ptr<CentroidUpdaterIf> makeForCentroidId(const std::string&) override;
};

} // centroid_update_worker
} // relevanced
