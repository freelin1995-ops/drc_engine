# drc-engine

A standalone **Design Rule Checking (DRC) engine** for semiconductor layout verification. Extracted and adapted from [KLayout](https://klayout.de/) (GPL v2, Matthias Koefferlein).

Loads GDSII layout files, runs DRC checks expressed in **Lua scripts**, and writes results to a new GDSII output file.

## Features

- **Boolean operations** — AND, OR, SUB, XOR on polygon layers
- **DRC checks** — width, space, notch, enclosure, separation, overlap
- **Geometry transforms** — sizing (grow/shrink), merge, edge extraction
- **Spatial filters** — interacting, inside, outside, enclosing
- **Edge operations** — extend, segment (center/start/end), corner detection
- **Area / perimeter** — measurement and filtering
- **Output** — write results to GDSII layers or report database
- **Lua scripting** — full API via sol2 bindings

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

# Run tests
cmake --build build --target test_engine && ./build/tests/test_engine
```

The CLI tool is built at `build/src/cli/drc-check`.

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

-- DRC checks (values in microns)
local width_violations = metal1:width(0.10)
local space_violations = metal1:space(0.10)

-- Sizing
local wide = metal1:sized(0.05)

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

**DRCLayer methods (all values in microns):**

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
| `r:sized(d)` / `r:sized(dx, dy)` | Region → Region | Sizing (grow/shrink) |
| `r:merge()` | Region → Region | Merge touching polygons |
| `r:edges()` | Region → Edges | Extract edges |
| `r:corners_dots(a1, a2)` | Region → Edges | Corner dots by angle range |
| `r:corners_boxes(dim, a1, a2)` | Region → Region | Corner boxes by angle range |
| `r:interacting(other)` | Region×Region → Region | Select interacting shapes |
| `r:inside(other)` | Region×Region → Region | Select shapes fully inside |
| `r:outside(other)` | Region×Region → Region | Select shapes not fully inside |
| `r:enclosing(other)` | Region×Region → Region | Select shapes that enclose |
| `r:with_area(min, max)` | Region → Region | Area filter |
| `r:with_perimeter(min, max)` | Region → Region | Perimeter filter |
| `e:extended_out(d)` | Edges → Region | Extend edges outward |
| `e:extended_in(d)` | Edges → Region | Extend edges inward |
| `e:extended(b, e, o, i, join)` | Edges → Region | Generic edge extension |
| `e:centers(l, f)` | Edges → Edges | Center segments |
| `e:start_segments(l, f)` | Edges → Edges | Start segments |
| `e:end_segments(l, f)` | Edges → Edges | End segments |
| `e:length()` | Edges → double | Total edge length |
| `r:count()` / `e:count()` / `ep:count()` | any → int | Number of elements |
| `r:empty()` / ... | any → bool | Whether empty |
| `r:area()` | Region → double | Total area |
| `r:perimeter()` | Region → double | Total perimeter |
| `x:output(layer, dtype)` | any → void | Write to output layer |

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
```

Each layer is a static library with a strict dependency direction: `tl → db → gds → rdb → drc → cli`.

### Modules

- **`tl`** — Basic toolbox (streams, math, string utilities)
- **`db`** — Core layout data model (`Layout`, `Cell`, `Region`, `Edges`, `EdgePairs`, `Texts`) and scanline-based boolean geometry engine (`EdgeProcessor`)
- **`gds`** — GDSII binary format reader and writer
- **`rdb`** — Report database for storing DRC violation results with categories and cell mappings
- **`drc`** — High-level DRC engine combining all modules with Lua bindings via sol2
- **`cli`** — Command-line entry point that reads a Lua script and runs it through the engine

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
│   └── drc/
├── lua/                    # Example DRC scripts
│   ├── example.drc
│   ├── alm_drc.lua
│   └── spatial_drc.lua
├── testdata/               # Test layouts and scripts
│   ├── layouts/            # 28 individual GDS test layouts
│   ├── run_tests.lua       # 19 Lua tests
│   ├── klayout_migration.lua  # 17 KLayout compatibility tests
│   ├── test_drc.gds
│   └── gen_testdata.py
├── tests/test_engine.cc    # C++ test
└── 3rd/sol2/               # Header-only Lua binding library (submodule)
```

## License

GNU General Public License v2.0. This project is derived from [KLayout](https://klayout.de/) (Copyright 2006-2026, Matthias Koefferlein).
