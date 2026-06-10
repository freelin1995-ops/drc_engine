#pragma once
#include <vector>
#include <string>
#include "drc/engine.h"

namespace drc {

std::vector<char> serialize_drclayer(const DRCLayer& layer);
DRCLayer deserialize_drclayer(const char* data, size_t size);

} // namespace drc
