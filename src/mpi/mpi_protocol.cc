#include "mpi/mpi_protocol.h"
#include <iostream>

namespace drc {

void mpi_send(int dest, MPIMsgType type, const void* data, int size) {
    MPIHeader header;
    header.type = static_cast<int32_t>(type);
    header.size = size;

    MPI_Send(&header, sizeof(header), MPI_BYTE, dest, 0, MPI_COMM_WORLD);
    if (size > 0 && data != nullptr) {
        MPI_Send(data, size, MPI_BYTE, dest, 0, MPI_COMM_WORLD);
    }
}

MPIMessage mpi_recv(int source) {
    MPIMessage msg;

    MPI_Status status;
    MPI_Recv(&msg.header, sizeof(MPIHeader), MPI_BYTE, source, 0,
             MPI_COMM_WORLD, &status);

    if (msg.header.size > 0) {
        msg.payload.resize(msg.header.size);
        MPI_Recv(msg.payload.data(), msg.header.size, MPI_BYTE, source, 0,
                 MPI_COMM_WORLD, &status);
    }

    return msg;
}

void mpi_broadcast(int root, MPIMsgType type, const void* data, int size) {
    int32_t type_val = static_cast<int32_t>(type);
    MPI_Bcast(&type_val, 1, MPI_INT32_T, root, MPI_COMM_WORLD);
    MPI_Bcast(&size, 1, MPI_INT32_T, root, MPI_COMM_WORLD);
    if (size > 0 && data != nullptr) {
        MPI_Bcast(const_cast<void*>(data), size, MPI_BYTE, root, MPI_COMM_WORLD);
    }
}

void mpi_gather(int root, const void* send_data, int send_size,
                std::vector<std::vector<char>>& recv_buffers) {
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    std::vector<int> recv_sizes(world_size);
    MPI_Gather(&send_size, 1, MPI_INT32_T,
               recv_sizes.data(), 1, MPI_INT32_T,
               root, MPI_COMM_WORLD);

    std::vector<int> displs(world_size);
    int total = 0;
    for (int i = 0; i < world_size; i++) {
        displs[i] = total;
        total += recv_sizes[i];
    }

    std::vector<char> recv_buf;
    recv_buffers.resize(world_size);

    if (root == 0) {
        for (int i = 0; i < world_size; i++) {
            recv_buffers[i].resize(recv_sizes[i]);
        }
        recv_buf.resize(total);
    }

    MPI_Gatherv(send_data, send_size, MPI_BYTE,
                recv_buf.data(), recv_sizes.data(), displs.data(), MPI_BYTE,
                root, MPI_COMM_WORLD);

    if (root == 0) {
        for (int i = 0; i < world_size; i++) {
            if (recv_sizes[i] > 0) {
                std::memcpy(recv_buffers[i].data(),
                           recv_buf.data() + displs[i], recv_sizes[i]);
            }
        }
    }
}

void mpi_broadcast_done(int num_workers) {
    for (int i = 1; i <= num_workers; i++) {
        mpi_send(i, MPIMsgType::DONE, nullptr, 0);
    }
}

} // namespace drc
