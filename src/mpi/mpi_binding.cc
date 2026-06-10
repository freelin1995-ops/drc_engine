#include "mpi/mpi_binding.h"
#include "mpi/mpi_serialize.h"
#include <limits>
#include <cstring>

namespace drc {

thread_local MPIContext* g_mpi_ctx = nullptr;
thread_local std::string g_current_expr;

void install_newindex_hook(sol::state& lua) {
    sol::table mt = lua.create_table();
    mt[sol::meta_function::new_index] = [&lua](sol::object, sol::object key, sol::object value) {
        if (!key.is<std::string>()) {
            lua.globals().raw_set(key, value);
            return;
        }
        std::string var = key.as<std::string>();

        lua.globals().raw_set(key, value);

        if (value.is<DRCLayer>() &&
            g_mpi_ctx && g_mpi_ctx->analyzer &&
            g_mpi_ctx->analyzer->has_downstream_refs(var, -1)) {
            DRCLayer& layer = value.as<DRCLayer&>();
            mpi_scatter_var(var, layer, g_mpi_ctx->tiles, g_mpi_ctx->halo, g_mpi_ctx->dbu);
        }
    };
    lua.globals().set(sol::metatable_key, mt);
}

void bind_drc_engine_mpi(sol::state& lua, DRCEngine& engine, MPIContext* ctx) {
    g_mpi_ctx = ctx;

    install_newindex_hook(lua);

    lua.set_function("__expr", [](const std::string& expr) {
        g_current_expr = expr;
    });

    lua.set_function("source", [&engine, ctx](const std::string& path) {
        engine.load_layout(path);
        ctx->dbu = engine.dbu();
    });

    lua.set_function("target", [&engine](const std::string& path) {
        engine.set_target_path(path);
    });

    lua.set_function("write", [&engine]() {
        engine.write_output("");
    });

    lua.set_function("input", [&engine](int layer, int dtype) -> DRCLayer {
        return engine.input(layer, dtype);
    });

    auto layertype = lua.new_usertype<DRCLayer>("DRCLayer");

    layertype[sol::meta_function::bitwise_and] = [](const DRCLayer&, const DRCLayer&) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype[sol::meta_function::bitwise_or] = [](const DRCLayer&, const DRCLayer&) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype[sol::meta_function::subtraction] = [](const DRCLayer&, const DRCLayer&) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype[sol::meta_function::bitwise_xor] = [](const DRCLayer&, const DRCLayer&) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };

    layertype["width"] = [](DRCLayer&, double) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["space"] = [](DRCLayer&, double) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["notch"] = [](DRCLayer&, double) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["sized"] = sol::overload(
        [](DRCLayer&, double) { return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers); },
        [](DRCLayer&, double, double) { return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers); }
    );
    layertype["merge"] = [](DRCLayer&) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["edges"] = [](DRCLayer&) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["interacting"] = [](DRCLayer&, const DRCLayer&) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["inside"] = [](DRCLayer&, const DRCLayer&) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["outside"] = [](DRCLayer&, const DRCLayer&) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["enclosing"] = [](DRCLayer&, const DRCLayer&) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["extended_out"] = [](DRCLayer&, double) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["extended_in"] = [](DRCLayer&, double) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["centers"] = [](DRCLayer&, double, double) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["start_segments"] = [](DRCLayer&, double, double) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["end_segments"] = [](DRCLayer&, double, double) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["extended"] = [](DRCLayer&, double, double, double, double, bool) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["enclosing_check"] = [](DRCLayer&, const DRCLayer&, double) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["sep_check"] = [](DRCLayer&, const DRCLayer&, double) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["overlap_check"] = [](DRCLayer&, const DRCLayer&, double) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["corners_dots"] = [](DRCLayer&, double, sol::optional<double>) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["corners_boxes"] = [](DRCLayer&, double, sol::optional<double>, sol::optional<double>) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };

    layertype["count"] = [](DRCLayer& self) -> size_t {
        if (self.type() == DRCLayer::Region && self.region()) return self.region()->count();
        if (self.type() == DRCLayer::Edges && self.edges()) return self.edges()->count();
        if (self.type() == DRCLayer::EdgePairs && self.edge_pairs()) return self.edge_pairs()->count();
        if (self.type() == DRCLayer::Texts && self.texts()) return self.texts()->count();
        return 0;
    };
    layertype["empty"] = [](DRCLayer& self) -> bool {
        if (self.type() == DRCLayer::Region && self.region()) return self.region()->empty();
        if (self.type() == DRCLayer::Edges && self.edges()) return self.edges()->empty();
        if (self.type() == DRCLayer::EdgePairs && self.edge_pairs()) return self.edge_pairs()->empty();
        if (self.type() == DRCLayer::Texts && self.texts()) return self.texts()->empty();
        return true;
    };
    layertype["area"] = [](DRCLayer& self) -> double {
        if (self.type() == DRCLayer::Region && self.region()) return self.region()->area();
        return 0.0;
    };
    layertype["perimeter"] = [](DRCLayer& self) -> double {
        if (self.type() == DRCLayer::Region && self.region()) return self.region()->perimeter();
        return 0.0;
    };
    layertype["with_area"] = [&engine](DRCLayer& self, double min, sol::optional<double> max) {
        double dbu = engine.dbu();
        double min_db = min / (dbu * dbu);
        double max_db = max.value_or(std::numeric_limits<double>::max()) / (dbu * dbu);
        return self.with_area(min_db, max_db);
    };
    layertype["with_perimeter"] = [&engine](DRCLayer& self, double min, sol::optional<double> max) {
        double dbu = engine.dbu();
        double min_db = min / dbu;
        double max_db = max.value_or(std::numeric_limits<double>::max()) / dbu;
        return self.with_perimeter(min_db, max_db);
    };
    layertype["length"] = [](DRCLayer& self) -> double {
        if (self.type() == DRCLayer::Edges && self.edges()) return self.edges()->length();
        return 0.0;
    };
    layertype["type"] = [](DRCLayer& self) {
        switch (self.type()) {
            case DRCLayer::Region: return std::string("region");
            case DRCLayer::Edges: return std::string("edges");
            case DRCLayer::EdgePairs: return std::string("edge_pairs");
            case DRCLayer::Texts: return std::string("texts");
        }
        return std::string("unknown");
    };
    layertype["output"] = [&engine](DRCLayer& self, int layer, int dtype) {
        self.output(*engine.target(), layer, dtype);
    };
}

DRCLayer mpi_evaluate_expr(const std::string& expr, int num_workers) {
    mpi_broadcast(0, MPIMsgType::EXECUTE_RHS, expr.data(), (int)expr.size());

    std::vector<DRCLayer> tile_results;
    for (int i = 0; i < num_workers; i++) {
        auto msg = mpi_recv(i + 1);
        if (msg.header.size > 0) {
            auto layer = deserialize_drclayer(msg.payload.data(), msg.payload.size());
            tile_results.push_back(std::move(layer));
        }
    }

    if (tile_results.empty()) return DRCLayer();

    DRCLayer merged = tile_results[0];
    for (size_t i = 1; i < tile_results.size(); i++) {
        merged = merged | tile_results[i];
    }
    return merged;
}

void mpi_scatter_var(const std::string& var_name, const DRCLayer& global_val,
                     const std::vector<db::Box>& tiles, double halo, double dbu) {
    for (int i = 0; i < (int)tiles.size(); i++) {
        auto tile_val = clip_to_tile(global_val, tiles[i], halo, dbu);

        auto serialized = serialize_drclayer(tile_val);

        std::vector<char> payload;
        int32_t name_len = (int32_t)var_name.size();
        payload.insert(payload.end(), (char*)&name_len, (char*)&name_len + 4);
        payload.insert(payload.end(), var_name.begin(), var_name.end());
        payload.insert(payload.end(), serialized.begin(), serialized.end());

        mpi_send(i + 1, MPIMsgType::UPDATE_VAR, payload.data(), (int)payload.size());
    }
}

DRCLayer clip_to_tile(const DRCLayer& global, const db::Box& tile, double halo, double dbu) {
    db::Coord halo_db = db::Coord(halo / dbu);
    db::Box expanded(tile.left() - halo_db, tile.bottom() - halo_db,
                     tile.right() + halo_db, tile.top() + halo_db);

    db::Region tile_region;
    tile_region.insert(expanded);

    if (global.is_null()) return DRCLayer();

    if (global.type() == DRCLayer::Region) {
        return DRCLayer(new db::Region(*global.region() & tile_region));
    } else if (global.type() == DRCLayer::Edges) {
        return global.interacting(DRCLayer(new db::Region(tile_region)));
    } else if (global.type() == DRCLayer::EdgePairs) {
        return global.interacting(DRCLayer(new db::Region(tile_region)));
    } else if (global.type() == DRCLayer::Texts) {
        auto result = std::make_unique<db::Texts>();
        for (auto it = global.texts()->begin(); !it.at_end(); ++it) {
            if (expanded.contains(it->box().center())) {
                result->insert(*it);
            }
        }
        return DRCLayer(result.release());
    }
    return DRCLayer();
}

} // namespace drc
