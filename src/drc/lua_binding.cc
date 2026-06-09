#include "drc/lua_binding.h"
#include <stdexcept>
#include "tl/tlException.h"

namespace drc {

void bind_drc_engine(sol::state& lua, DRCEngine& engine) {
    lua.set_function("source", [&engine](const std::string& path) {
        try {
            return engine.load_layout(path);
        } catch (tl::Exception& e) {
            throw std::runtime_error(std::string("tl::Exception: ") + e.msg());
        } catch (std::exception& e) {
            throw std::runtime_error(std::string("std::exception: ") + e.what());
        } catch (...) {
            throw std::runtime_error("unknown exception loading layout");
        }
    });

    lua.set_function("target", [&engine](const std::string& path) {
        engine.set_target_path(path);
    });

    lua.set_function("write", [&engine]() {
        engine.write_output("");
    });

    lua.set_function("input", [&engine](int layer, int dtype) {
        return engine.input(layer, dtype);
    });

    // Convert microns to database units
    auto to_db = [&engine](double um) { return engine.to_db(um); };

    auto layer_type = lua.new_usertype<DRCLayer>("DRCLayer",
        sol::meta_function::bitwise_and, &DRCLayer::operator&,
        sol::meta_function::bitwise_or,  &DRCLayer::operator|,
        sol::meta_function::subtraction, &DRCLayer::operator-,
        sol::meta_function::bitwise_xor, &DRCLayer::operator^
    );

    // DRC checks: values are in microns, convert to db units
    layer_type["width"]     = [to_db](DRCLayer& self, double d) { return self.width_check(to_db(d)); };
    layer_type["space"]     = [to_db](DRCLayer& self, double d) { return self.space_check(to_db(d)); };
    layer_type["notch"]     = [to_db](DRCLayer& self, double d) { return self.notch_check(to_db(d)); };

    layer_type["sized"]     = sol::overload(
        [to_db](DRCLayer& self, double d) { return self.sized(to_db(d)); },
        [to_db](DRCLayer& self, double dx, double dy) { return self.sized(to_db(dx), to_db(dy)); }
    );
    layer_type["merge"]     = [](DRCLayer& self) { return self.merge(); };
    layer_type["edges"]     = [](DRCLayer& self) { return self.edges_op(); };
    layer_type["corners_dots"]   = [](DRCLayer& self, double a1, sol::optional<double> a2) { return self.corners_dots(a1, a2.value_or(180.0)); };
    layer_type["corners_boxes"]  = [to_db](DRCLayer& self, double dim, sol::optional<double> a1, sol::optional<double> a2) {
        return self.corners_boxes(to_db(dim), a1.value_or(-180.0), a2.value_or(180.0));
    };

    layer_type["interacting"] = [](DRCLayer& self, const DRCLayer& other) { return self.interacting(other); };
    layer_type["inside"]      = [](DRCLayer& self, const DRCLayer& other) { return self.inside(other); };
    layer_type["outside"]     = [](DRCLayer& self, const DRCLayer& other) { return self.outside(other); };
    layer_type["enclosing"]   = [](DRCLayer& self, const DRCLayer& other) { return self.enclosing(other); };

    // Edge operations (on Edges type)
    layer_type["extended_out"] = [to_db](DRCLayer& self, double d) { return self.extended_out(to_db(d)); };
    layer_type["extended_in"]  = [to_db](DRCLayer& self, double d) { return self.extended_in(to_db(d)); };
    layer_type["length"]       = [](DRCLayer& self) -> double {
        if (self.type() == DRCLayer::Edges && self.edges()) return self.edges()->length();
        return 0.0;
    };
    layer_type["centers"]       = [to_db](DRCLayer& self, double l, double f) { return self.centers(to_db(l), f); };
    layer_type["start_segments"] = [to_db](DRCLayer& self, double l, double f) { return self.start_segments(to_db(l), f); };
    layer_type["end_segments"]   = [to_db](DRCLayer& self, double l, double f) { return self.end_segments(to_db(l), f); };
    layer_type["extended"] = [to_db](DRCLayer& self, double b, double e, double o, double i, bool join) {
        return self.extended(to_db(b), to_db(e), to_db(o), to_db(i), join);
    };

    // DRC check methods (return EdgePairs)
    layer_type["enclosing_check"] = [to_db](DRCLayer& self, const DRCLayer& other, double d) {
        return self.enclosure_check(other, to_db(d));
    };
    layer_type["sep_check"] = [to_db](DRCLayer& self, const DRCLayer& other, double d) {
        return self.separation_check(other, to_db(d));
    };
    layer_type["overlap_check"] = [to_db](DRCLayer& self, const DRCLayer& other, double d) {
        return self.overlap_check(other, to_db(d));
    };

    // Query methods
    layer_type["count"] = [](DRCLayer& self) -> size_t {
        if (self.type() == DRCLayer::Region && self.region()) return self.region()->count();
        if (self.type() == DRCLayer::Edges && self.edges()) return self.edges()->count();
        if (self.type() == DRCLayer::EdgePairs && self.edge_pairs()) return self.edge_pairs()->count();
        return 0;
    };
    layer_type["empty"] = [](DRCLayer& self) -> bool {
        if (self.type() == DRCLayer::Region && self.region()) return self.region()->empty();
        if (self.type() == DRCLayer::Edges && self.edges()) return self.edges()->empty();
        if (self.type() == DRCLayer::EdgePairs && self.edge_pairs()) return self.edge_pairs()->empty();
        return true;
    };
    layer_type["area"] = [](DRCLayer& self) -> double {
        if (self.type() == DRCLayer::Region && self.region()) return self.region()->area();
        return 0.0;
    };
    layer_type["perimeter"] = [](DRCLayer& self) -> double {
        if (self.type() == DRCLayer::Region && self.region()) return self.region()->perimeter();
        return 0.0;
    };

    // Area filter: min/max are in square microns, convert to db units squared
    layer_type["with_area"] = [&engine](DRCLayer& self, double min, sol::optional<double> max) {
        double dbu = engine.dbu();
        double min_db = min / (dbu * dbu);
        double max_db = max.value_or(std::numeric_limits<double>::max()) / (dbu * dbu);
        return self.with_area(min_db, max_db);
    };

    // Perimeter filter: min/max are in microns (not db units)
    layer_type["with_perimeter"] = [&engine](DRCLayer& self, double min, sol::optional<double> max) {
        double dbu = engine.dbu();
        double min_db = min / dbu;
        double max_db = max.value_or(std::numeric_limits<double>::max()) / dbu;
        return self.with_perimeter(min_db, max_db);
    };

    layer_type["type"] = [](DRCLayer& self) {
        switch (self.type()) {
            case DRCLayer::Region: return std::string("region");
            case DRCLayer::Edges: return std::string("edges");
            case DRCLayer::EdgePairs: return std::string("edge_pairs");
            case DRCLayer::Texts: return std::string("texts");
        }
        return std::string("unknown");
    };

    layer_type["output"] = [&engine](DRCLayer& self, int layer, int dtype) {
        self.output(*engine.target(), layer, dtype);
    };
}

} // namespace drc
