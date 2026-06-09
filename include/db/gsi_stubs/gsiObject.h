#pragma once

#include "tlCommon.h"

namespace gsi {

class GSI_PUBLIC ObjectBase {
public:
  enum StatusEventType {
    ObjectDestroyed = 0,
    ObjectKeep = 1,
    ObjectRelease = 2
  };

  ObjectBase() {}
  ObjectBase(const ObjectBase &) {}
  virtual ~ObjectBase() = default;

  void keep() {}
};

} // namespace gsi
