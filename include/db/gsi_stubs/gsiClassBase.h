#pragma once

#include "tlVariant.h"

namespace gsi {

class GSI_PUBLIC ClassBase {
public:
  virtual ~ClassBase() = default;
  virtual const tl::VariantUserClassBase *var_cls(bool) const { return nullptr; }
};

} // namespace gsi
