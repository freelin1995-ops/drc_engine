# Build Environment Reference

## System Overview

| Item | Value |
|------|-------|
| OS | Fedora Linux 44 (Workstation Edition) x86_64 |
| Kernel | 7.0.11-200.fc44.x86_64 (PREEMPT_DYNAMIC) |
| Hostname | fedora |
| CPU | AMD Ryzen 5 9500F (6 cores, 12 threads) |
| RAM | 30 GiB |
| Disk | 300 GB NVMe (220 GB free) |

---

## Compiler

| Item | Value |
|------|-------|
| C++ | GCC 16.1.1 20260515 (Red Hat 16.1.1-2) |
| C | GCC 16.1.1 20260515 (Red Hat 16.1.1-2) |
| Binary (C++) | `/usr/bin/g++` |
| Binary (C) | `/usr/bin/gcc` |
| glibc | 2.43 |
| C++ Standard | C++17 |
| Clang | Not installed |

---

## Build System

| Tool | Version | Binary |
|------|---------|--------|
| CMake | 4.3.0 | `/usr/bin/cmake` |
| GNU Make | 4.4.1 | `/usr/bin/make` |
| Ninja | 1.13.2 | `/usr/bin/ninja` |
| pkg-config | 2.5.1 | `/usr/bin/pkg-config` |
| Git | 2.53.0 | `/usr/bin/git` |
| Python | 3.14.3 | `/usr/bin/python3` |

---

## Dependencies

### Lua

| Item | Value |
|------|-------|
| Version | 5.4.8 |
| RPM | `lua-5.4.8-5.fc44`, `lua-devel-5.4.8-5.fc44`, `lua-libs-5.4.8-5.fc44` |
| Headers | `/usr/include/lua.h`, `/usr/include/lua.hpp`, `/usr/include/lualib.h` |
| Library | `/usr/lib64/liblua-5.4.so` |
| CMake | `find_package(Lua REQUIRED)` |
| `LUA_INCLUDE_DIR` | `/usr/include` |
| `LUA_LIBRARY` | `/usr/lib64/liblua-5.4.so` |

### sol2 (Lua C++ binding)

| Item | Value |
|------|-------|
| Version | 3.5.0 |
| Source | Git submodule at `3rd/sol2` (commit `c1f95a77`) |
| Include path | `3rd/sol2/include/` |
| CMake symlink | `3rd/sol -> sol2/include` |
| Header | `3rd/sol2/include/sol/sol.hpp` |
| Build | Added as `INTERFACE` library `sol`, no compilation needed |

### ZLIB

| Item | Value |
|------|-------|
| Package | `zlib-ng-compat-2.3.3-3.fc44`, `zlib-ng-compat-devel-2.3.3-3.fc44` |
| CMake | `find_package(ZLIB REQUIRED)` |

### OpenMPI (optional — for distributed mode)

| Item | Value |
|------|-------|
| Version | 5.0.9 (OpenMPI) |
| RPM | `openmpi-5.0.9-4.fc44`, `openmpi-devel-5.0.9-4.fc44` |
| Compiler wrapper | `/usr/lib64/openmpi/bin/mpicxx` |
| Headers | `/usr/include/openmpi-x86_64/mpi.h` |
| Library dir | `/usr/lib64/openmpi/lib/libmpi.so` |
| Runtime | `/usr/lib64/openmpi/bin/mpirun` |
| `prterun` | `/usr/lib64/openmpi/bin/prterun` (not in PATH by default) |
| MPI compiler flags | `-I/usr/include/openmpi-x86_64` |
| MPI link flags | `-L/usr/lib64/openmpi/lib -lmpi -Wl,-rpath,/usr/lib64/openmpi/lib` |
| CMake var | `DRC_USE_MPI:BOOL=ON` |
| CMake MPI find | `find_package(MPI REQUIRED COMPONENTS CXX)` — uses compiler wrapper detection |
| Note | On Fedora, `mpicxx` is NOT in PATH by default. CMake's `FindMPI` auto-detects it at `/usr/lib64/openmpi/bin/mpicxx` |
| Runtime note | `mpirun` needs `OMPI_PRTERUN=/usr/lib64/openmpi/bin/prterun` or PATH to include `/usr/lib64/openmpi/bin` |

### KLayout (embedded)

| Item | Value |
|------|-------|
| Type | Source vendored in repository (not a system package) |
| Libraries | `src/tl/`, `src/db/`, `src/gds/`, `src/rdb/` |
| Built targets | `libtl.a`, `libdb.a`, `libgds.a`, `librdb.a` |
| No system `klayout-devel` needed | All source is local |

---

## Build Configurations

### Non-MPI build

```
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build
```

CMake cache at `build/CMakeCache.txt`:
- `DRC_USE_MPI=OFF`
- `CMAKE_CXX_COMPILER=/usr/bin/c++`

### MPI build

```
cmake -B build_mpi -DCMAKE_BUILD_TYPE=Debug -DDRC_USE_MPI=ON
cmake --build build_mpi -j$(nproc)
OMPI_PRTERUN=/usr/lib64/openmpi/bin/prterun \
  mpirun -np 5 build_mpi/src/cli/drc-check --mpi-tiles 2x2 <script.lua>
```

CMake cache at `build_mpi/CMakeCache.txt`:
- `DRC_USE_MPI=ON`
- `MPI_CXX_COMPILER=/usr/lib64/openmpi/bin/mpicxx`
- `MPI_CXX_COMPILER_INCLUDE_DIRS=/usr/include/openmpi-x86_64`

---

## Repo Layout

```
./
├── 3rd/
│   └── sol2/           # Git submodule: sol3 v3.5.0
│   └── sol -> sol2/include  # symlink for CMake
├── include/drc/        # Public headers (engine.h)
├── src/
│   ├── tl/             # KLayout tl (foundation) library
│   ├── db/             # KLayout db (database) library
│   ├── gds/            # KLayout gds (GDS I/O) library
│   ├── rdb/            # KLayout rdb (report database) library
│   ├── drc/            # DRC engine core (engine.cc, lua_binding.cc)
│   ├── mpi/            # MPI distributed mode (master, worker, protocol, script_analyzer, halo_inferrer, serialization)
│   └── cli/            # CLI entry point (main.cc)
├── tests/              # C++ tests (test_engine.cc, test_integration.cc, test_mpi_serialize.cc, test_script_analyzer.cc)
├── testdata/           # Lua test scripts & GDS layouts
│   ├── layouts/        # 32 GDS test layouts
│   ├── run_tests.lua
│   ├── mpi_scenarios.lua
│   ├── test_edge_cases.lua
│   └── klayout_migration.lua
├── cmake/              # Custom CMake modules
├── docs/               # Documentation & specs
├── packaging/          # Arch Linux PKGBUILD (not tracked in this branch)
├── build/              # Non-MPI build output
├── build_mpi/          # MPI build output
└── .venv/              # Python venv for test layout generation
```

---

## Runtime Environment Variables

| Variable | Value | Required for |
|----------|-------|-------------|
| `OMPI_PRTERUN` | `/usr/lib64/openmpi/bin/prterun` | `mpirun` (OpenMPI on Fedora 44) |
| `LD_LIBRARY_PATH` | (not set — rpath handles it) | Not needed (built with `-Wl,-rpath`) |

---

## Package install commands

For a fresh Fedora 44 system:

```bash
# Base build tools
dnf install -y gcc-c++ cmake make ninja-build git pkgconf

# Required libraries
dnf install -y lua-devel zlib-ng-compat-devel

# MPI (optional)
dnf install -y openmpi-devel

# Debugging & tools
dnf install -y gdb valgrind

# Python (for test layout regeneration)
dnf install -y python3
```

After cloning:
```bash
git clone --recurse-submodules <repo-url>
cd drc-engine
# For MPI runtime: add OpenMPI bin to PATH or use OMPI_PRTERUN
export PATH="/usr/lib64/openmpi/bin:$PATH"
```

---

## Sol2 notes

sol2 is a **header-only** library. Running `git submodule update --init 3rd/sol2` is sufficient — no compilation needed. CMake adds it as an `INTERFACE` library:

```cmake
add_library(sol INTERFACE)
target_include_directories(sol INTERFACE ${CMAKE_SOURCE_DIR}/3rd/sol)
```

The symlink `3rd/sol -> sol2/include` exists for convenience.

---

## Test execution

```bash
# Non-MPI tests
ctest --test-dir build -V

# Manual test with a specific script
build/src/cli/drc-check testdata/run_tests.lua

# MPI test
OMPI_PRTERUN=/usr/lib64/openmpi/bin/prterun \
  mpirun -np 5 build_mpi/src/cli/drc-check --mpi-tiles 2x2 testdata/mpi_scenarios.lua

# MPI requires working directory to be project root (GDS paths are relative)
```
