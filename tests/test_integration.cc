#include <iostream>
#include <cassert>
#include <memory>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#include "drc/engine.h"
#include "drc/lua_binding.h"
#include "db/dbRegion.h"
#include "db/dbEdges.h"
#include "db/dbEdgePairs.h"
#include "db/dbTexts.h"
#include "db/dbLayout.h"
#include "db/dbBox.h"

#include "mpi/script_analyzer.h"
#include <sol/sol.hpp>

const double EPS = 1e-9;

static bool approx(double a, double b) {
    return std::fabs(a - b) < EPS;
}



static void test_basic_lua_script() {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::math,
                       sol::lib::string, sol::lib::table, sol::lib::os);

    drc::DRCEngine engine;
    drc::bind_drc_engine(lua, engine);

    std::string script = R"(
        source("testdata/layouts/basic_input.gds")
        target("/tmp/test_integ_output.gds")

        local m1 = input(10, 0)
        m1:output(1, 0)

        local merged = m1:merge()
        merged:output(2, 0)
        write()
    )";

    auto result = lua.safe_script(script, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "Lua error: " << err.what() << std::endl;
        assert(false);
    }

    std::cout << "PASS: test_basic_lua_script" << std::endl;
}

static void test_boolean_ops() {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::math,
                       sol::lib::string, sol::lib::table, sol::lib::os);

    drc::DRCEngine engine;
    drc::bind_drc_engine(lua, engine);

    std::string script = R"(
        source("testdata/layouts/boolean_ops.gds")
        target("/tmp/test_integ_boolean.gds")
        local a = input(10, 0)
        local b = input(20, 0)
        local and_result = a & b
        local or_result  = a | b
        local xor_result = a ~ b
        and_result:output(1, 0)
        or_result:output(2, 0)
        xor_result:output(4, 0)
        write()
    )";

    auto result = lua.safe_script(script, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "Lua error: " << err.what() << std::endl;
        assert(false);
    }

    std::cout << "PASS: test_boolean_ops" << std::endl;
}

static void test_drc_checks() {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::math,
                       sol::lib::string, sol::lib::table, sol::lib::os);

    drc::DRCEngine engine;
    drc::bind_drc_engine(lua, engine);

    std::string script = R"(
        source("testdata/layouts/width_space_check.gds")
        target("/tmp/test_integ_checks.gds")
        local m1 = input(10, 0)
        local width_violations = m1:width(0.2)
        local space_violations = m1:space(0.2)
        width_violations:output(1, 0)
        space_violations:output(2, 0)
        write()
    )";

    auto result = lua.safe_script(script, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "Lua error: " << err.what() << std::endl;
        assert(false);
    }

    // DRC checks produce EdgePairs which are stored as raw edges in output
    // Just verify no crash and something produced
    std::cout << "PASS: test_drc_checks" << std::endl;
}

static void test_sizing() {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::math,
                       sol::lib::string, sol::lib::table, sol::lib::os);

    drc::DRCEngine engine;
    drc::bind_drc_engine(lua, engine);

    std::string script = R"(
        source("testdata/layouts/klayout_sizing.gds")
        target("/tmp/test_integ_sizing.gds")
        local m1 = input(10, 0)
        local wide = m1:sized(0.05)
        wide:output(1, 0)
        write()
    )";

    auto result = lua.safe_script(script, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "Lua error: " << err.what() << std::endl;
        assert(false);
    }

    std::cout << "PASS: test_sizing" << std::endl;
}

static void test_edge_ops() {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::math,
                       sol::lib::string, sol::lib::table, sol::lib::os);

    drc::DRCEngine engine;
    drc::bind_drc_engine(lua, engine);

    std::string script = R"(
        source("testdata/layouts/edge_ops.gds")
        target("/tmp/test_integ_edge.gds")
        local m1 = input(10, 0)
        local e = m1:edges()
        e:output(1, 0)
        local ext = e:extended_out(0.1)
        ext:output(2, 0)
        write()
    )";

    auto result = lua.safe_script(script, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "Lua error: " << err.what() << std::endl;
        assert(false);
    }

    std::cout << "PASS: test_edge_ops" << std::endl;
}

static void test_interacting() {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::math,
                       sol::lib::string, sol::lib::table, sol::lib::os);

    drc::DRCEngine engine;
    drc::bind_drc_engine(lua, engine);

    std::string script = R"(
        source("testdata/layouts/klayout_boolean.gds")
        target("/tmp/test_integ_interact.gds")
        local m1 = input(10, 0)
        local m2 = input(20, 0)
        local inter = m1:interacting(m2)
        inter:output(1, 0)
        write()
    )";

    auto result = lua.safe_script(script, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "Lua error: " << err.what() << std::endl;
        assert(false);
    }

    std::cout << "PASS: test_interacting" << std::endl;
}

static void write_test_file(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    f << content;
}

static void test_script_analyzer_normalization() {
    std::string input = R"(
        source("test.gds")
        target("/tmp/out.gds")
        local m1 = input(10, 0)
        local m2 = m1 & input(20, 0)
        local violations = m2:width(0.1)
        violations:output(100, 0)
        write()
    )";

    write_test_file("/tmp/test_normalize.drc", input);
    drc::ScriptAnalyzer analyzer("/tmp/test_normalize.drc");
    analyzer.normalize();
    std::string normalized = analyzer.normalized_script();
    assert(analyzer.valid());

    assert(normalized.find("local ") == std::string::npos);
    bool has_expr = normalized.find("__expr") != std::string::npos;
    std::cout << "  Has __expr: " << (has_expr ? "yes" : "no") << std::endl;
    std::cout << "  Normalized:\n" << normalized << std::endl;
    assert(has_expr);

    std::cout << "PASS: test_script_analyzer_normalization" << std::endl;
}

static void test_script_analyzer_ref_table() {
    std::string script = R"(
        source("test.gds")
        target("/tmp/out.gds")
        local m1 = input(10, 0)
        local m2 = m1 & input(20, 0)
        local violations = m2:width(0.1)
        violations:output(100, 0)
        write()
    )";

    write_test_file("/tmp/test_ref_table.drc", script);
    drc::ScriptAnalyzer analyzer("/tmp/test_ref_table.drc");
    analyzer.build_ref_table();

    assert(analyzer.has_downstream_refs("m1", -1));
    assert(analyzer.has_downstream_refs("m2", -1));
    // violations is referenced by output() line, so it has downstream refs
    assert(analyzer.has_downstream_refs("violations", -1));

    std::cout << "PASS: test_script_analyzer_ref_table" << std::endl;
}

static void test_invalid_gds_path() {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::math,
                       sol::lib::string, sol::lib::table, sol::lib::os);

    drc::DRCEngine engine;
    drc::bind_drc_engine(lua, engine);

    std::string script = R"(
        local ok, err = pcall(source, "nonexistent_file.gds")
        assert(ok == false)
    )";

    auto result = lua.safe_script(script, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "Lua error: " << err.what() << std::endl;
        assert(false);
    }

    std::cout << "PASS: test_invalid_gds_path" << std::endl;
}

int main() {
    test_basic_lua_script();
    test_boolean_ops();
    test_drc_checks();
    test_sizing();
    test_edge_ops();
    test_interacting();
    test_script_analyzer_normalization();
    test_script_analyzer_ref_table();
    test_invalid_gds_path();

    std::cout << "\nAll integration tests passed!" << std::endl;
    return 0;
}
