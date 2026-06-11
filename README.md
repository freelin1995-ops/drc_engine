# drc-engine

A standalone **Design Rule Checking (DRC) engine** for semiconductor layout verification. Extracted and adapted from [KLayout](https://klayout.de/) (GPL v2, Matthias Koefferlein).

Loads GDSII layout files, runs DRC checks expressed in **Lua scripts**, and writes results to a new GDSII output file.

## Features

- **Boolean operations** — AND, OR, SUB, XOR on polygon layers and edge sets
- **DRC checks** — width, space, notch, enclosure, separation, overlap
- **Geometry transforms** — sizing (grow/shrink), merge, edge extraction
- **Corner detection** — mark corners by angle range as dots or boxes
- **Spatial filters** — interacting, inside, outside, enclosing
- **Edge operations** — extend, segment (center/start/end), length query
- **Area / perimeter** — measurement and filtering
- **Output** — write results to GDSII layers
- **Lua scripting** — full API via sol2 bindings
- **MPI distributed mode** (optional) — parallel DRC execution across workers
- **Script analyzer** — normalizes and analyzes DRC scripts for distributed processing

## Building

### Prerequisites

- CMake >= 3.16
- C++17 compiler
- [Lua](https://www.lua.org/) (dev headers)
- ZLIB (dev headers)

### Build

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run C++ tests
cmake --build build --target test_engine && ./build/tests/test_engine
cmake --build build --target test_integration && ./build/tests/test_integration

# Run Lua tests
./build/src/cli/drc-check testdata/run_tests.lua
```

The CLI tool is built at `build/src/cli/drc-check`.

### MPI distributed build (optional)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DDRC_USE_MPI=ON
cmake --build build
```

## Usage

```bash
./build/src/cli/drc-check lua/example.drc
```

### Example DRC script

```lua
-- Load input and set output
source("testdata/test_drc.gds")
target("/tmp/output.gds")

-- Read layers
local metal1 = input(10, 0)
local block  = input(30, 0)

-- Boolean operations
local overlap = metal1 & block
local merged  = metal1 | block
local diff    = metal1 - block
local xor_res = metal1 ~ block

-- DRC checks (values in microns)
local width_violations  = metal1:width(0.10)
local space_violations  = metal1:space(0.10)
local notch_violations  = metal1:notch(0.10)

-- Sizing (isotropic and anisotropic)
local wide      = metal1:sized(0.05)
local stretched = metal1:sized(0.05, 0.10)

-- Spatial filters
local inter = metal1:interacting(block)
local inside = metal1:inside(block)

-- Edge operations
local edges = metal1:edges()
local ext   = edges:extended_out(0.05)
local len   = edges:length()

-- Properties
print("Count: " .. metal1:count())
print("Area:  " .. metal1:area())
print("Type:  " .. metal1:type())

-- Write results to output layers
metal1:output(1, 0)
block:output(2, 0)
overlap:output(10, 0)
width_violations:output(100, 0)
space_violations:output(101, 0)

write()
```

### Lua API Reference

**Global functions:**

| Function | Description |
|----------|-------------|
| `source(path)` | Load a GDS layout file |
| `target(path)` | Set output GDS path |
| `input(layer, dtype)` | Read shapes from a layer as a `DRCLayer` |
| `write()` | Flush results to the output file |

**DRCLayer methods (dimensional values in microns, auto-converted to database units):**

| Method | Input → Output | Description |
|--------|---------------|-------------|
| `a & b` | Region×Region → Region | Boolean AND |
| `a \| b` | Region×Region → Region | Boolean OR |
| `a - b` | Region×Region → Region | Boolean SUB |
| `a ~ b` | Region×Region → Region | Boolean XOR |
| `r:width(d)` | Region → EdgePairs | Minimum width check |
| `r:space(d)` | Region → EdgePairs | Minimum space check |
| `r:notch(d)` | Region → EdgePairs | Notch check |
| `r:enclosing_check(other, d)` | Region×Region → EdgePairs | Enclosure check |
| `r:sep_check(other, d)` | Region×Region → EdgePairs | Separation check |
| `r:overlap_check(other, d)` | Region×Region → EdgePairs | Overlap check |
| `r:sized(d)` | Region → Region | Isotropic sizing (grow/shrink) |
| `r:sized(dx, dy)` | Region → Region | Anisotropic sizing |
| `r:merge()` | Region → Region | Merge touching polygons |
| `r:edges()` | Region → Edges | Extract edges |
| `r:corners_dots(a1)` | Region → Edges | Corner dots (convex = -90°, second param defaults to 180) |
| `r:corners_dots(a1, a2)` | Region → Edges | Corner dots by angle range [a1, a2] |
| `r:corners_boxes(dim, a1?, a2?)` | Region → Region | Corner boxes (default a1=-180, a2=180) |
| `r:interacting(other)` | Region×Region → Region | Select interacting shapes |
| `r:inside(other)` | Region×Region → Region | Select shapes fully inside |
| `r:outside(other)` | Region×Region → Region | Select shapes not fully inside |
| `r:enclosing(other)` | Region×Region → Region | Select shapes that enclose |
| `r:with_area(min, max?)` | Region → Region | Area filter (single-arg = lower bound only) |
| `r:with_perimeter(min, max?)` | Region → Region | Perimeter filter (single-arg = lower bound only) |
| `e:extended_out(d)` | Edges → Region | Extend edges outward |
| `e:extended_in(d)` | Edges → Region | Extend edges inward |
| `e:extended(b, e, o, i, join)` | Edges → Region | Generic edge extension |
| `e:centers(l, f)` | Edges → Edges | Center segments |
| `e:start_segments(l, f)` | Edges → Edges | Start segments |
| `e:end_segments(l, f)` | Edges → Edges | End segments |
| `e:length()` | Edges → double | Total edge length (in db units) |
| `r:count()` / `e:count()` / `ep:count()` | any → int | Number of elements |
| `r:empty()` / ... | any → bool | Whether empty |
| `r:area()` | Region → double | Total area (in db²) |
| `r:perimeter()` | Region → double | Total perimeter (in db units) |
| `x:type()` | any → string | Type name: `"region"`, `"edges"`, `"edge_pairs"`, `"texts"` |
| `x:output(layer, dtype)` | any → void | Write to output layer |

**Boolean and selection operators also work on Edges** (`a & b`, `a | b`, `a - b`, `a ~ b`, `interacting`, `inside`, `outside`).

**EdgePairs** support `|` (join) and `interacting(other)`.

**Enclosing vs. enclosure check:** `enclosing()` returns Region polygons that enclose other polygons; `enclosing_check()` returns EdgePairs where enclosure is less than a threshold.

## Architecture

```
  ┌──────────────┐
  │     drc      │  Lua bindings + DRCEngine/DRCLayer API
  ├──────────────┤
  │     rdb      │  Report database for violations
  ├──────────────┤
  │     gds      │  GDSII format reader/writer
  ├──────────────┤
  │     db       │  Layout data model + geometry engine
  ├──────────────┤
  │     tl       │  Toolbox: streams, math, strings
  └──────────────┘
      ┌──────┐
      │ mpi  │  Distributed DRC (optional)
      └──────┘
```

Each layer is a static library with a strict dependency direction: `tl → db → gds → rdb → drc → cli`. The `mpi` module is optional and depends on MPI.

### Modules

- **`tl`** — Basic toolbox (streams, math, string utilities, logging, threading, heap, containers)
- **`db`** — Core layout data model (`Layout`, `Cell`, `Region`, `Edges`, `EdgePairs`, `Texts`) and scanline-based boolean geometry engine (`EdgeProcessor`)
- **`gds`** — GDSII binary format reader and writer
- **`rdb`** — Report database for storing DRC violation results with categories and cell mappings
- **`drc`** — High-level DRC engine combining all modules with Lua bindings via sol2
- **`cli`** — Command-line entry point that reads a Lua script and runs it through the engine
- **`mpi`** (optional) — Distributed DRC support: master/worker orchestration, halo inference, script analysis, serialization

### Script Analyzer

The `ScriptAnalyzer` (`src/mpi/script_analyzer.cc`) normalizes Lua DRC scripts for MPI distributed processing:
- Decomposes chained calls into single-assignment form with temp variables
- Injects `__expr()` calls to capture expression strings
- Builds reference tables for variable dependency analysis
- Detects input/output/write lines

## Project Structure

```
├── CMakeLists.txt          # Top-level build
├── include/                # Public headers (matching module layout)
│   ├── tl/
│   ├── db/
│   ├── gds/
│   ├── rdb/
│   └── drc/
├── src/                    # Module implementations
│   ├── cli/main.cc         # Entry point
│   ├── tl/
│   ├── db/
│   ├── gds/
│   ├── rdb/
│   ├── drc/
│   └── mpi/                # Distributed DRC support
│       ├── mpi_master.cc/.h
│       ├── mpi_worker.cc/.h
│       ├── mpi_protocol.cc/.h
│       ├── mpi_binding.cc/.h
│       ├── mpi_serialize.cc/.h
│       ├── halo_inferrer.cc/.h
│       └── script_analyzer.cc/.h
├── lua/                    # Example DRC scripts
│   ├── example.drc
│   ├── alm_drc.lua
│   └── spatial_drc.lua
├── testdata/               # Test layouts and scripts
│   ├── layouts/            # 19 GDS test layouts
│   ├── scripts/
│   ├── run_tests.lua       # 20 Lua tests
│   ├── klayout_migration.lua  # 17 KLayout compatibility tests
│   ├── gen_testdata.py
│   └── gen_test_layouts.py
├── tests/                  # C++ tests
│   ├── test_engine.cc
│   ├── test_integration.cc     # 9 integration tests
│   ├── test_mpi_serialize.cc   # MPI serialization test
│   ├── test_script_analyzer.cc # 3 ScriptAnalyzer tests
│   ├── gen_large_layout.cc     # Large layout generator
│   └── run_mpi_integration.sh
├── scripts/
│   └── check_output.py     # GDS output verification script
└── 3rd/
    ├── sol/                 # sol2 header-only Lua binding library (submodule)
    └── sol2/                # (alternate)
```

## Testing

The project includes three test suites:

| Suite | File | Tests | Description |
|-------|------|-------|-------------|
| Lua tests | `testdata/run_tests.lua` | 20 | Boolean ops, DRC checks, sizing, edge ops, corner detection, selection filters |
| KLayout migration | `testdata/klayout_migration.lua` | 17 | Compatibility with KLayout DRC script semantics |
| C++ integration | `tests/test_integration.cc` | 9 | Lua script execution, ScriptAnalyzer, error handling |
| C++ engine | `tests/test_engine.cc` | 1 | Basic engine smoke test |
| ScriptAnalyzer | `tests/test_script_analyzer.cc` | 3 | Chain decomposition, ref table, expression injection |
| MPI serialization | `tests/test_mpi_serialize.cc` | 1 | MPI data serialization |

```bash
# Run all C++ tests
cmake --build build --target test_engine && ./build/tests/test_engine
cmake --build build --target test_integration && ./build/tests/test_integration
cmake --build build --target test_script_analyzer && ./build/tests/test_script_analyzer

# Run Lua tests
./build/src/cli/drc-check testdata/run_tests.lua
./build/src/cli/drc-check testdata/klayout_migration.lua
```

## License

GNU General Public License v2.0. This project is derived from [KLayout](https://klayout.de/) (Copyright 2006-2026, Matthias Koefferlein).
