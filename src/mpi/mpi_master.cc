#include "mpi/mpi_master.h"
#include "mpi/mpi_binding.h"
#include "mpi/mpi_protocol.h"
#include "mpi/script_analyzer.h"
#include "drc/engine.h"
#include <iostream>
#include <mpi.h>
#include <sol/sol.hpp>

namespace drc {

int run_master(int argc, char* argv[]) {
    // --- Parse CLI args ---
    std::string script_path;
    TileConfig tile_config{1, 1};
    double halo_override = -1.0;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--mpi-tiles" && i + 1 < argc) {
            std::string val = argv[++i];
            auto x_pos = val.find('x');
            if (x_pos != std::string::npos) {
                tile_config.nx = std::stoi(val.substr(0, x_pos));
                tile_config.ny = std::stoi(val.substr(x_pos + 1));
            }
        } else if (arg == "--halo" && i + 1 < argc) {
            halo_override = std::stod(argv[++i]);
        } else if (arg[0] != '-') {
            script_path = arg;
        }
    }

    if (script_path.empty()) {
        std::cerr << "Usage: drc-check --mpi-tiles NxN [--halo d] script.drc" << std::endl;
        return 1;
    }

    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    int num_workers = world_size - 1;
    if (num_workers < 1) {
        std::cerr << "Need at least 2 MPI processes (1 master + 1 worker)" << std::endl;
        return 1;
    }

    int total_tiles = tile_config.nx * tile_config.ny;
    if (total_tiles > num_workers) {
        std::cerr << "Tile count (" << total_tiles
                  << ") exceeds worker count (" << num_workers << ")" << std::endl;
        return 1;
    }
    if (total_tiles < num_workers) {
        std::cerr << "Warning: tile count (" << total_tiles
                  << ") < worker count (" << num_workers
                  << "). Some workers will be idle." << std::endl;
    }

    // --- Phase 1: Script analysis ---
    ScriptAnalyzer analyzer(script_path);
    analyzer.normalize();
    analyzer.build_ref_table();

    // --- Phase 2: Halo inference ---
    double halo;
    if (halo_override >= 0.0) {
        halo = halo_override;
    } else {
        std::cerr << "Warning: no halo inferrer available, using 0.0" << std::endl;
        halo = 0.0;
    }

    // --- Phase 3: Setup engine + MPI bindings ---
    DRCEngine engine;
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::math,
                       sol::lib::string, sol::lib::table);

    MPIContext ctx;
    ctx.num_workers = num_workers;
    ctx.halo = halo;
    ctx.analyzer = &analyzer;

    bind_drc_engine_mpi(lua, engine, &ctx);

    // Override source() to also compute tile grid
    lua.set_function("source", [&engine, &ctx, &tile_config](const std::string& path) {
        engine.load_layout(path);
        ctx.dbu = engine.dbu();

        auto* layout = engine.layout();
        auto top_cell = *layout->begin_top_down();
        const auto& bbox = layout->cell(top_cell).bbox();

        double tile_w = (double)(bbox.right() - bbox.left()) / tile_config.nx;
        double tile_h = (double)(bbox.top() - bbox.bottom()) / tile_config.ny;

        ctx.tiles.clear();
        for (int y = 0; y < tile_config.ny; y++) {
            for (int x = 0; x < tile_config.nx; x++) {
                db::Coord x1 = bbox.left() + (db::Coord)(tile_w * x);
                db::Coord y1 = bbox.bottom() + (db::Coord)(tile_h * y);
                db::Coord x2 = (x == tile_config.nx - 1) ? bbox.right() : (db::Coord)(tile_w * (x + 1));
                db::Coord y2 = (y == tile_config.ny - 1) ? bbox.top() : (db::Coord)(tile_h * (y + 1));
                ctx.tiles.push_back(db::Box(x1, y1, x2, y2));
            }
        }
    });

    // --- Phase 4: Execute normalized script ---
    auto result = lua.safe_script(analyzer.normalized_script(),
                                  sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "Master script error: " << err.what() << std::endl;
        return 1;
    }

    // --- Phase 5: Signal workers to finish ---
    mpi_broadcast_done(num_workers);

    return 0;
}

} // namespace drc
