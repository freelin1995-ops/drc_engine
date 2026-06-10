#pragma once
#include <string>
#include <vector>

namespace drc {

struct TileConfig {
    int nx;
    int ny;
};

int run_master(int argc, char* argv[]);

} // namespace drc
