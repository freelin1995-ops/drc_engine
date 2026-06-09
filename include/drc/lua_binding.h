#pragma once
#include <sol/sol.hpp>
#include "drc/engine.h"

namespace drc {

void bind_drc_engine(sol::state& lua, DRCEngine& engine);

} // namespace drc
