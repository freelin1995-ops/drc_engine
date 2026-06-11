#include "mpi/mpi_worker.h"
#include "mpi/mpi_protocol.h"
#include "mpi/mpi_serialize.h"
#include "drc/lua_binding.h"
#include "drc/engine.h"
#include <iostream>
#include <cstring>
#include <sol/sol.hpp>

namespace drc {

int run_worker() {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::math,
                       sol::lib::string, sol::lib::table);

    DRCEngine engine;
    bind_drc_engine(lua, engine);

    while (true) {
        auto msg = mpi_recv(0);
        MPIMsgType type = static_cast<MPIMsgType>(msg.header.type);

        switch (type) {
        case MPIMsgType::EXECUTE_RHS: {
            std::string expr(msg.payload.begin(), msg.payload.end());

            auto result = lua.safe_script(
                "return " + expr, sol::script_pass_on_error);
            if (!result.valid()) {
                sol::error err = result;
                int rank;
                MPI_Comm_rank(MPI_COMM_WORLD, &rank);
                std::string err_msg = "rank " + std::to_string(rank) +
                    " | " + expr + " | " + err.what();
                std::cerr << "Worker RHS error: " << err_msg << std::endl;
                mpi_send(0, MPIMsgType::WORKER_ERROR,
                         err_msg.data(), (int)err_msg.size());
                break;
            }

            DRCLayer layer = result;
            auto serialized = serialize_drclayer(layer);
            mpi_send(0, MPIMsgType::EXECUTE_RHS,
                     serialized.data(), (int)serialized.size());
            break;
        }
        case MPIMsgType::UPDATE_VAR: {
            if (msg.header.size < 4) break;
            int32_t name_len;
            std::memcpy(&name_len, msg.payload.data(), 4);
            if (name_len < 0 || (int32_t)msg.payload.size() < 4 + name_len) break;

            std::string var_name(msg.payload.data() + 4, name_len);
            int data_offset = 4 + name_len;
            int data_size = (int)msg.payload.size() - data_offset;

            if (data_size > 0) {
                DRCLayer layer = deserialize_drclayer(
                    msg.payload.data() + data_offset, data_size);
                lua.globals()[var_name] = std::move(layer);
            }
            break;
        }
        case MPIMsgType::DONE:
            return 0;
        default:
            std::cerr << "Worker: unknown message type " << (int)type << std::endl;
            return 1;
        }
    }
}

} // namespace drc
