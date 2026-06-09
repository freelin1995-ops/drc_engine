#include <iostream>
#include <fstream>
#include <sstream>
#include <sol/sol.hpp>
#include "drc/lua_binding.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: drc-check <script.lua>" << std::endl;
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "Error: Cannot open script file: " << argv[1] << std::endl;
        return 1;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    std::string script = ss.str();

    sol::state lua;
    lua.open_libraries(
        sol::lib::base,
        sol::lib::math,
        sol::lib::string,
        sol::lib::table,
        sol::lib::os
    );

    drc::DRCEngine engine;
    drc::bind_drc_engine(lua, engine);

    auto result = lua.safe_script(script, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "DRC script error: " << err.what() << std::endl;
        return 1;
    }

    return 0;
}
