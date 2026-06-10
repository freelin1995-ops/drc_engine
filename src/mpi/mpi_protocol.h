#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include <mpi.h>

namespace drc {

enum class MPIMsgType : int {
    EXECUTE_RHS = 0,
    UPDATE_VAR  = 1,
    DONE        = 2,
};

#pragma pack(push, 1)
struct MPIHeader {
    int32_t type;
    int32_t size;
};
#pragma pack(pop)

struct MPIMessage {
    MPIHeader header;
    std::vector<char> payload;

    std::string expr() const {
        return std::string(payload.data(), payload.size());
    }

    std::string var_name() const {
        if (payload.size() < 4) return {};
        int32_t len;
        std::memcpy(&len, payload.data(), 4);
        return std::string(payload.data() + 4, len);
    }

    const char* data() const {
        return payload.data();
    }
};

void mpi_send(int dest, MPIMsgType type, const void* data, int size);
MPIMessage mpi_recv(int source);
void mpi_broadcast(int root, MPIMsgType type, const void* data, int size);
void mpi_gather(int root, const void* send_data, int send_size,
                std::vector<std::vector<char>>& recv_buffers);
void mpi_broadcast_done(int num_workers);

} // namespace drc
