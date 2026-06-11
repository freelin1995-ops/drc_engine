# Worker Error Propagation Design

## Problem

When a worker fails to evaluate an expression (e.g., missing scattered variable), it sends an empty message to the master. The master (`mpi_evaluate_expr`) sees `msg.header.size == 0` and silently skips that tile's result. **No warning or error is shown**, and the user gets incomplete DRC results without knowing.

## Decision: Warn + Continue

On worker failure, master prints all tile errors and continues with successful tiles. Rationale: partial results are useful for debugging; user can see which tiles failed and why.

## Design: New `WORKER_ERROR` Message Type

### 1. Protocol (`mpi_protocol.h`)

```cpp
enum class MPIMsgType : int {
    EXECUTE_RHS = 0,
    UPDATE_VAR  = 1,
    DONE        = 2,
    WORKER_ERROR = 3,   // NEW
};
```

No changes to `MPIHeader` or `MPIMessage`.

### 2. Worker (`mpi_worker.cc`)

When `lua.safe_script("return " + expr)` fails:

```cpp
// Before: sends empty EXECUTE_RHS (silent drop)
// mpi_send(0, MPIMsgType::EXECUTE_RHS, nullptr, 0);

// After: sends WORKER_ERROR with error details
int rank;
MPI_Comm_rank(MPI_COMM_WORLD, &rank);
auto err_msg = "rank " + std::to_string(rank) + " | " + expr + " | " + err.what();
mpi_send(0, MPIMsgType::WORKER_ERROR, err_msg.data(), (int)err_msg.size());
```

### 3. Master (`mpi_binding.cc` — `mpi_evaluate_expr`)

```cpp
int errors = 0;
for (int i = 0; i < active; i++) {
    int worker_rank = i + 1;
    auto msg = mpi_recv(worker_rank);
    if (msg.header.type == (int32_t)MPIMsgType::WORKER_ERROR) {
        std::string err(msg.payload.begin(), msg.payload.end());
        std::cerr << "Warning: tile " << i << " (rank " << worker_rank
                  << ") failed: " << err << std::endl;
        errors++;
        continue;
    }
    if (msg.header.size > 0) {
        auto layer = deserialize_drclayer(msg.payload.data(), msg.payload.size());
        tile_results.push_back(std::move(layer));
    }
}
```

## Edge Cases

| Case | Behavior |
|------|----------|
| Some tiles fail | Warning per-failure, continue with successful tiles |
| All tiles fail | Warnings + empty result returned |
| Worker crashes | Not handled here — depends on H3 (MPI error checking) |

## Files Changed

- `src/mpi/mpi_protocol.h` — add `WORKER_ERROR = 3`
- `src/mpi/mpi_worker.cc:30-33` — send `WORKER_ERROR` instead of empty `EXECUTE_RHS`
- `src/mpi/mpi_binding.cc:198-212` — check msg type in `mpi_evaluate_expr`
