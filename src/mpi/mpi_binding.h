#pragma once
#include <sol/sol.hpp>
#include <vector>
#include <string>
#include "drc/engine.h"
#include "script_analyzer.h"
#include "mpi_protocol.h"

namespace drc {

struct MPIContext {
    int num_workers;
    std::vector<db::Box> tiles;
    double halo;
    double dbu;
    ScriptAnalyzer* analyzer;
};

extern thread_local MPIContext* g_mpi_ctx;
extern thread_local std::string g_current_expr;

void bind_drc_engine_mpi(sol::state& lua, DRCEngine& engine, MPIContext* ctx);

void mpi_scatter_var(const std::string& var_name, const DRCLayer& global_val,
                     const std::vector<db::Box>& tiles, double halo, double dbu);

DRCLayer mpi_evaluate_expr(const std::string& expr, int num_workers);

DRCLayer clip_to_tile(const DRCLayer& global, const db::Box& tile, double halo, double dbu);

} // namespace drc
