#pragma once

#include "gsiClassBase.h"

namespace gsi {

template <class X>
const ClassBase *cls_decl()
{
  static ClassBase instance;
  return &instance;
}

} // namespace gsi
