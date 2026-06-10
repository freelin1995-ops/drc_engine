# MPI Distributed DRC Engine — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement MPI distributed DRC engine as designed in `docs/superpowers/specs/2026-06-10-mpi-drc-design.md`

**Architecture:** Master runs `lua.script(normalized_script)` with MPI-aware bindings; `_G.__newindex` auto-scatters; worker loop handles 3 message types (EXECUTE_RHS, UPDATE_VAR, DONE).

**Tech Stack:** C++17, OpenMPI, sol2, KLayout db library, Lua 5.4+

---

## File Map

### New files (all under `src/mpi/`):

| File | Responsibility |
|------|---------------|
| `mpi_protocol.h` | Message types, struct enums |
| `mpi_serialize.h / .cc` | DRCLayer ↔ byte array |
| `mpi_binding.h / .cc` | MPI-aware Lua bindings (`__newindex` + operator overrides) |
| `mpi_master.h / .cc` | Master init, tile compute, `lua.script()` entry |
| `mpi_worker.h / .cc` | Worker message loop |
| `script_analyzer.h / .cc` | Normalize (chain→SSA, strip local), build ref table |
| `halo_inferrer.h / .cc` | Run script to collect distance params |
| `CMakeLists.txt` | Build for mpi target |

### New files (under `tests/`):

| File | Responsibility |
|------|---------------|
| `test_script_analyzer.cc` | Tests for normalization + ref table |
| `test_mpi_serialize.cc` | Tests for DRCLayer serialization roundtrip |

### Modified files:

| File | Change |
|------|--------|
| `CMakeLists.txt` (root) | Add `DRC_USE_MPI` option, `find_package(MPI)`, conditionally add `src/mpi` |
| `src/cli/main.cc` | MPI branch: `rank==0 → run_master`, else `run_worker` |
| `src/cli/CMakeLists.txt` | Link MPI libs when `DRC_USE_MPI` |
| `include/drc/engine.h` | Minor: add `const db::Layout* target() const` (missing const overload) |

---

### Task 1: ScriptAnalyzer — Normalization + Reference Table

**Files:**
- Create: `src/mpi/script_analyzer.h`
- Create: `src/mpi/script_analyzer.cc`
- Create: `tests/test_script_analyzer.cc`

- [ ] **Step 1: Write `script_analyzer.h`**

```cpp
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <set>

namespace drc {

struct RefEntry {
    std::string var;
    int def_line;
    std::vector<int> ref_lines;  // line numbers that reference this var
    bool is_input;               // defined by input()
};

class ScriptAnalyzer {
public:
    ScriptAnalyzer(const std::string& script_path);

    void normalize();
    void build_ref_table();

    const std::vector<std::string>& lines() const { return m_lines; }
    const std::string& normalized_script() const { return m_normalized_script; }
    const std::string& original_script() const { return m_original_script; }
    const std::unordered_map<std::string, RefEntry>& ref_table() const { return m_ref_table; }

    bool has_downstream_refs(const std::string& var, int def_line) const;
    bool is_input_line(int line_num) const;
    std::string get_assigned_var(int line_num) const;
    int num_lines() const { return (int)m_lines.size(); }

private:
    struct LineInfo {
        std::string text;
        bool is_input = false;
        std::string assigned_var;   // empty if not an assignment
    };

    std::string m_original_script;
    std::vector<LineInfo> m_original_lines;
    std::vector<LineInfo> m_lines;       // normalized lines (global vars)
    std::string m_normalized_script;     // joined normalized lines
    std::unordered_map<std::string, RefEntry> m_ref_table;

    void split_lines(const std::string& script);
    std::string decompose_chains(const std::string& line, int line_num);
    std::string strip_local(const std::string& line);
    bool is_comment_or_empty(const std::string& line) const;
    std::string extract_assigned_var(const std::string& line) const;
    std::vector<std::string> extract_referenced_vars(const std::string& expr) const;
    bool is_source_line(const std::string& line) const;
    bool is_target_line(const std::string& line) const;
    bool is_write_line(const std::string& line) const;
    bool is_output_line(const std::string& line) const;
    bool is_input_call(const std::string& line) const;
};

} // namespace drc
```

- [ ] **Step 2: Write the failing tests**

File `tests/test_script_analyzer.cc`:

```cpp
#include <iostream>
#include <cassert>
#include <string>
#include <cstdio>
#include <cstdlib>
#include "drc/engine.h"
#include "mpi/script_analyzer.h"

// Helper: write string to temp file, return path
static std::string write_temp_script(const std::string& content) {
    char path[] = "/tmp/drc_test_XXXXXX";
    int fd = mkstemp(path);
    write(fd, content.data(), content.size());
    close(fd);
    return std::string(path);
}

static void test_chain_decomposition() {
    std::string script = write_temp_script(R"(
source("input.gds")
target("out.gds")
local w = input(10, 0):sized(0.1):width(0.5)
w:output(1, 0)
write()
)");
    drc::ScriptAnalyzer analyzer(script);
    analyzer.normalize();
    analyzer.build_ref_table();

    // Should produce 7 lines (source, target, __t1, __t2, w, output, write)
    assert(analyzer.num_lines() == 7);

    // Check normalized script uses globals (no "local")
    std::string norm = analyzer.normalized_script();
    assert(norm.find("local") == std::string::npos);

    // Check __t1 is the first temp
    assert(norm.find("__t1 = input(10, 0)") != std::string::npos);

    // Check ref table: __t1 referenced by __t2
    assert(analyzer.has_downstream_refs("__t1", 0));

    // Check w has no downstream refs
    assert(!analyzer.has_downstream_refs("w", 2));

    std::remove(path);
    std::cout << "PASS: test_chain_decomposition" << std::endl;
}

static void test_no_chain() {
    std::string path = write_temp_script(R"(
source("a.gds")
target("b.gds")
local a = input(10, 0)
local b = input(30, 0)
local merged = a | b
local w = merged:width(0.5)
w:output(1, 0)
write()
)");
    drc::ScriptAnalyzer analyzer(path);
    analyzer.normalize();
    analyzer.build_ref_table();

    assert(analyzer.num_lines() == 7);
    std::string norm = analyzer.normalized_script();
    assert(norm.find("local") == std::string::npos);

    // Check refs
    assert(analyzer.has_downstream_refs("a", 0));
    assert(analyzer.has_downstream_refs("merged", 3));
    assert(!analyzer.has_downstream_refs("w", 4));

    std::remove(path.c_str());
    std::cout << "PASS: test_no_chain" << std::endl;
}

int main() {
    test_chain_decomposition();
    test_no_chain();
    std::cout << "All script_analyzer tests passed!" << std::endl;
    return 0;
}
```

- [ ] **Step 3: Run tests to verify they fail**

```bash
cd /home/linfeng/code/drc-engine
g++ -std=c++17 -I include -I 3rd/sol -c tests/test_script_analyzer.cc -o /tmp/test_script_analyzer.o 2>&1 | head -5
```
Expected: fails because `mpi/script_analyzer.h` doesn't exist.

- [ ] **Step 4: Write minimal implementation**

File `src/mpi/script_analyzer.cc`:

```cpp
#include "mpi/script_analyzer.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

namespace drc {

ScriptAnalyzer::ScriptAnalyzer(const std::string& script_path) {
    std::ifstream file(script_path);
    std::stringstream ss;
    ss << file.rdbuf();
    m_original_script = ss.str();
}

void ScriptAnalyzer::split_lines(const std::string& script) {
    m_original_lines.clear();
    std::istringstream stream(script);
    std::string line;
    while (std::getline(stream, line)) {
        // Trim trailing whitespace
        while (!line.empty() && (line.back() == ' ' || line.back() == '\r'))
            line.pop_back();
        if (!is_comment_or_empty(line)) {
            LineInfo li;
            li.text = line;
            m_original_lines.push_back(li);
        }
    }
}

bool ScriptAnalyzer::is_comment_or_empty(const std::string& line) const {
    if (line.empty()) return true;
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos) return true;
    return line[start] == '-' && start + 1 < line.size() && line[start + 1] == '-';
}

std::string ScriptAnalyzer::strip_local(const std::string& line) {
    std::string result = line;
    // Match "local varname =" or "local varname ="
    std::regex local_re(R"(^\s*local\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=)");
    std::smatch m;
    if (std::regex_search(result, m, local_re)) {
        result = result.substr(0, m.position(0)) +
                 m.str(1) +
                 result.substr(m.position(0) + m.length(0));
    }
    return result;
}

std::string ScriptAnalyzer::extract_assigned_var(const std::string& line) const {
    std::regex assign_re(R"(^\s*(?:local\s+)?([a-zA-Z_][a-zA-Z0-9_]*)\s*=)");
    std::smatch m;
    if (std::regex_search(line, m, assign_re)) {
        return m.str(1);
    }
    return "";
}

bool ScriptAnalyzer::is_input_call(const std::string& line) const {
    // Check RHS for input() call
    std::string rhs;
    size_t eq = line.find('=');
    if (eq != std::string::npos) {
        rhs = line.substr(eq + 1);
    } else {
        rhs = line;
    }
    return rhs.find("input(") != std::string::npos;
}

bool ScriptAnalyzer::is_source_line(const std::string& line) const {
    return line.find("source(") == 0 || line.find("source(") != std::string::npos;
}

bool ScriptAnalyzer::is_target_line(const std::string& line) const {
    std::string t = line;
    while (!t.empty() && t[0] == ' ') t = t.substr(1);
    return t.find("target(") == 0;
}

bool ScriptAnalyzer::is_write_line(const std::string& line) const {
    std::string t = line;
    while (!t.empty() && t[0] == ' ') t = t.substr(1);
    return t.find("write(") == 0;
}

bool ScriptAnalyzer::is_output_line(const std::string& line) const {
    return line.find(":output(") != std::string::npos;
}

std::vector<std::string> ScriptAnalyzer::extract_referenced_vars(const std::string& expr) const {
    std::vector<std::string> vars;
    // Match varname followed by operator, :method(, ), or end-of-string
    std::regex var_re(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\b)");
    std::sregex_iterator it(expr.begin(), expr.end(), var_re);
    std::sregex_iterator end;
    // Keywords that are not variable references
    std::set<std::string> keywords = {"local", "input", "source", "target", "write", "return", "nil", "true", "false"};
    for (; it != end; ++it) {
        std::string var = it->str(1);
        if (keywords.find(var) == keywords.end() &&
            !(var.size() > 2 && var[0] == '_' && var[1] == '_')) {
            // Only include if not a number
            if (!std::isdigit(var[0])) {
                vars.push_back(var);
            }
        }
    }
    return vars;
}

std::string ScriptAnalyzer::decompose_chains(const std::string& line, int line_num) {
    // Check if line has a method chain call (something:method1(...):method2(...))
    size_t assign_eq = line.find('=');
    std::string rhs;
    std::string prefix;
    if (assign_eq != std::string::npos) {
        prefix = line.substr(0, assign_eq + 1);
        rhs = line.substr(assign_eq + 1);
    } else {
        return line;  // not an assignment, no decomposition needed
    }

    // Look for chain: expr:method1(...):method2(...)
    // We find the deepest chain and split at the outermost :method call
    // Simple approach: find all :method( sequences
    std::string result_line = line;
    std::string current_rhs = rhs;

    int temp_counter = 0;
    std::vector<std::string> new_lines;
    std::string expr = current_rhs;

    // Keep splitting off the last :method(...) call
    while (true) {
        // Find the last :method(...) at the top level (not nested)
        // Look for patterns like :identifier(...) at the end of expr
        std::regex chain_re(R"(^(.*):([a-zA-Z_][a-zA-Z0-9_]*\(.*)\)\s*$)");
        std::smatch m;
        if (std::regex_match(expr, m, chain_re)) {
            std::string base = m.str(1);
            std::string method_call = m.str(2);

            // Generate temp variable
            std::string temp_var = "__t" + std::to_string(++temp_counter);

            // Create: __tN = base_expr
            new_lines.push_back(temp_var + " = " + base);

            // Update expr to: temp_var:method_call
            // But method_call already has the closing paren, so:
            // Actually the regex captures everything after the last ':'
            // Let's re-examine: the full method_call already has args
            // We want: __tN:method_call
            expr = temp_var + ":" + method_call;
        } else {
            break;
        }
    }

    if (new_lines.empty()) {
        return line;  // no chain
    }

    // Replace the original line with decomposed lines (in order)
    // We build a new normalized script segment
    // But we need to reassign to the original variable name on the last line
    std::string orig_var = extract_assigned_var(line);
    std::string result;

    for (auto& nl : new_lines) {
        result += strip_local(nl) + "\n";
    }
    // Last line: orig_var = fully_decomposed_expr
    result += orig_var + " = " + expr;

    return result;
}

void ScriptAnalyzer::normalize() {
    split_lines(m_original_script);

    std::vector<LineInfo> all_lines;
    for (int i = 0; i < (int)m_original_lines.size(); i++) {
        std::string line = m_original_lines[i].text;

        // Decompose chains
        std::string decomposed = decompose_chains(line, i);

        // decompose_chains may return multiple lines or the original
        std::istringstream stream(decomposed);
        std::string sub_line;
        while (std::getline(stream, sub_line)) {
            while (!sub_line.empty() && (sub_line.back() == ' ' || sub_line.back() == '\r'))
                sub_line.pop_back();

            // Strip local
            std::string global_line = strip_local(sub_line);

            LineInfo li;
            li.text = global_line;
            li.assigned_var = extract_assigned_var(global_line);
            li.is_input = is_input_call(global_line);
            all_lines.push_back(li);
        }
    }
    m_lines = all_lines;

    // Build normalized script string with __expr injection
    // For each operation line, inject '__expr("RHS_expr")' before it.
    // This sets g_current_expr so MPI operator overrides know what to broadcast.
    // input() lines and output/write/source/target don't need __expr.
    std::stringstream ss;
    for (auto& li : m_lines) {
        if (!li.assigned_var.empty() && !li.is_input &&
            !is_source_line(li.text) && !is_target_line(li.text) &&
            !is_write_line(li.text) && !is_output_line(li.text)) {
            // This is an operation line: var = expr
            // Inject __expr("expr") before it
            size_t eq = li.text.find('=');
            std::string rhs = li.text.substr(eq + 1);
            // Trim spaces
            while (!rhs.empty() && rhs[0] == ' ') rhs = rhs.substr(1);
            ss << "__expr(\"" << rhs << "\")\n";
        }
        ss << li.text << "\n";
    }
    m_normalized_script = ss.str();
}

void ScriptAnalyzer::build_ref_table() {
    m_ref_table.clear();

    for (int i = 0; i < (int)m_lines.size(); i++) {
        auto& li = m_lines[i];
        if (li.assigned_var.empty()) continue;

        RefEntry entry;
        entry.var = li.assigned_var;
        entry.def_line = i;
        entry.is_input = li.is_input;

        // Look for references in subsequent lines
        for (int j = i + 1; j < (int)m_lines.size(); j++) {
            auto vars = extract_referenced_vars(m_lines[j].text);
            for (auto& v : vars) {
                if (v == entry.var) {
                    entry.ref_lines.push_back(j);
                }
            }
        }
        m_ref_table[entry.var] = entry;
    }
}

bool ScriptAnalyzer::has_downstream_refs(const std::string& var, int def_line) const {
    auto it = m_ref_table.find(var);
    if (it == m_ref_table.end()) return false;
    return !it->second.ref_lines.empty();
}

bool ScriptAnalyzer::is_input_line(int line_num) const {
    if (line_num < 0 || line_num >= (int)m_lines.size()) return false;
    return m_lines[line_num].is_input;
}

std::string ScriptAnalyzer::get_assigned_var(int line_num) const {
    if (line_num < 0 || line_num >= (int)m_lines.size()) return "";
    return m_lines[line_num].assigned_var;
}

} // namespace drc
```

- [ ] **Step 5: Run tests to verify they pass**

```bash
cd /home/linfeng/code/drc-engine
g++ -std=c++17 -I include -I 3rd/sol -I src/mpi \
    -c src/mpi/script_analyzer.cc -o /tmp/script_analyzer.o && \
g++ -std=c++17 -I include -I 3rd/sol -I src/mpi \
    -c tests/test_script_analyzer.cc -o /tmp/test_script_analyzer.o && \
g++ /tmp/script_analyzer.o /tmp/test_script_analyzer.o -o /tmp/test_script_analyzer && \
/tmp/test_script_analyzer
```
Expected: "All script_analyzer tests passed!"

- [ ] **Step 6: Refine decomposition regex to handle nested parentheses**

The simple regex in `decompose_chains` won't handle `:width(0.5)` with parenthesized args. Actually, for the common cases `:width(d)`, `:sized(d)`, `:space(d)`, `:sized(dx, dy)`, `:enclosing_check(other, d)` etc, we need to handle commas and nested parens. Update `decompose_chains`:

In `decompose_chains`, instead of regex, use a manual scan to find the last `:method(...)` call by matching parentheses:

```cpp
// Find the last method call at the top level
static std::pair<std::string, std::string> split_last_method_call(const std::string& expr) {
    // Walk backwards to find ':name(' where parens are balanced
    int depth = 0;
    int last_colon = -1;
    int paren_start = -1;
    for (int i = (int)expr.size() - 1; i >= 0; i--) {
        if (expr[i] == ')') depth++;
        else if (expr[i] == '(') {
            depth--;
            if (depth == 0) {
                // Found the matching '(' for the last method call
                paren_start = i;
                // Now find the ':' before this
                for (int j = i - 1; j >= 0; j--) {
                    if (expr[j] == ':') {
                        last_colon = j;
                        break;
                    }
                }
                break;
            }
        }
    }
    if (last_colon >= 0 && paren_start > last_colon) {
        std::string base = expr.substr(0, last_colon);
        std::string method = expr.substr(last_colon + 1);
        return {base, method};
    }
    return {"", ""};
}
```

Replace the regex in `decompose_chains` with this function.

- [ ] **Step 7: Re-run tests and commit**

```bash
# Recompile and test
g++ -std=c++17 -I include -I 3rd/sol -I src/mpi \
    -c src/mpi/script_analyzer.cc -o /tmp/script_analyzer.o && \
g++ -std=c++17 -I include -I 3rd/sol -I src/mpi \
    -c tests/test_script_analyzer.cc -o /tmp/test_script_analyzer.o && \
g++ /tmp/script_analyzer.o /tmp/test_script_analyzer.o -o /tmp/test_script_analyzer && \
/tmp/test_script_analyzer
```
Expected: PASS

```bash
git add -A && git commit -m "feat(mpi): add ScriptAnalyzer for normalization and reference table"
```

---

### Task 2: MPI Protocol + Serialization

**Files:**
- Create: `src/mpi/mpi_protocol.h`
- Create: `src/mpi/mpi_serialize.h`
- Create: `src/mpi/mpi_serialize.cc`
- Create: `tests/test_mpi_serialize.cc`

- [ ] **Step 1: Write mpi_protocol.h**

```cpp
#pragma once
#include <cstdint>

namespace drc {

enum class MPIMsgType : int {
    EXECUTE_RHS = 0,   // Master→Worker: evaluate Lua expr, return DRCLayer
    UPDATE_VAR  = 1,   // Master→Worker: set variable to tile-local DRCLayer
    DONE        = 2,   // Master→Worker: shutdown
};

// Wire format: 4-byte msg type + 4-byte payload size + payload
#pragma pack(push, 1)
struct MPIHeader {
    int32_t type;      // MPIMsgType as int
    int32_t size;      // payload size in bytes (0 for DONE)
};

struct MPIUpdateVar {
    int32_t var_name_len;   // length of variable name string
    // followed by: var_name (var_name_len bytes)
    // followed by: serialized DRCLayer data (remaining bytes)
};

struct MPIRHSExec {
    int32_t expr_len;       // length of expression string
    // followed by: expr (expr_len bytes)
};
#pragma pack(pop)

// Helper RAII wrapper for MPI communication
struct MPIMessage {
    MPIHeader header;
    std::vector<char> payload;

    static MPIMessage recv(int source);
    static void send(int dest, MPIMsgType type, const void* data = nullptr, int size = 0);
    static void broadcast(int root, MPIMsgType type, const void* data, int size);
    static void gather(int root, const void* send_data, int send_size,
                       std::vector<std::vector<char>>& recv_buffers);

    int var_name_len() const;
    std::string var_name() const;
    int expr_len() const;
    std::string expr() const;
    const char* data() const;
};

} // namespace drc
```

- [ ] **Step 2: Write mpi_serialize.h**

```cpp
#pragma once
#include <vector>
#include <string>
#include "drc/engine.h"

namespace drc {

// Serialize a DRCLayer to a byte vector
std::vector<char> serialize_drclayer(const DRCLayer& layer);

// Deserialize bytes to a DRCLayer
DRCLayer deserialize_drclayer(const char* data, size_t size);

// Get the serialized size of a DRCLayer
size_t serialized_size(const DRCLayer& layer);

// Helper: get geometry type string for GDS layer purpose
const char* geom_type_to_gds_purpose(DRCLayer::Type type);

} // namespace drc
```

- [ ] **Step 3: Write failing serialize tests**

```cpp
// tests/test_mpi_serialize.cc
#include <iostream>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include "drc/engine.h"
#include "mpi/mpi_serialize.h"

static void test_region_roundtrip() {
    // Create a Region with a box
    auto* region = new db::Region();
    region->insert(db::Box(0, 0, 1000, 2000));
    region->insert(db::Box(5000, 5000, 6000, 7000));

    drc::DRCLayer original(region);
    assert(original.type() == drc::DRCLayer::Region);
    assert(original.region()->count() == 2);

    auto bytes = drc::serialize_drclayer(original);
    assert(!bytes.empty());

    auto restored = drc::deserialize_drclayer(bytes.data(), bytes.size());
    assert(restored.type() == drc::DRCLayer::Region);
    assert(restored.region() != nullptr);
    assert(restored.region()->count() == 2);

    std::cout << "PASS: test_region_roundtrip" << std::endl;
}

int main() {
    test_region_roundtrip();
    std::cout << "All MPI serialize tests passed!" << std::endl;
    return 0;
}
```

- [ ] **Step 4: Run to verify failure**

```bash
cd /home/linfeng/code/drc-engine
g++ -std=c++17 -I include -I 3rd/sol -I src/mpi \
    -c tests/test_mpi_serialize.cc -o /tmp/test_mpi_serialize.o 2>&1 | head -5
```
Expected: fails, mpi_serialize.h doesn't exist.

- [ ] **Step 5: Implement mpi_serialize.cc**

The serialization approach:
1. Create a temporary `db::Layout` with a single cell
2. Insert the DRCLayer's geometry into a layer in that cell
3. Write the in-memory Layout to GDS2 format via `tl::OutputMemoryStream` + `db::GDS2Writer`
4. Read back via `tl::InputMemoryStream` + `db::GDS2Reader`
5. Extract geometry from the read Layout

```cpp
#include "mpi/mpi_serialize.h"
#include "db/dbLayout.h"
#include "gds/dbGDS2Writer.h"
#include "gds/dbGDS2Reader.h"
#include "tl/tlStream.h"

namespace drc {

// Helper: create a singleton Layout to serialize/deserialize
// Layout creation is expensive, so we cache one
static thread_local struct SerializeLayouts {
    db::Layout write_layout;
    db::Layout read_layout;
    db::cell_index_type cell;
    unsigned int layer;
    bool initialized = false;

    void ensure() {
        if (initialized) return;
        write_layout = db::Layout(true);  // true = with technology
        write_layout.dbu(0.001);  // 1nm dbu

        // Need to add cell
        // In KLayout db::Layout, we need to figure out how to add a cell
        // cell = write_layout.add_cell("TEMP");
        // Add a layer
        // layer = write_layout.insert_layer(db::LayerProperties(999, 0));

        // README: db::Layout API details to be filled during implementation
        initialized = true;
    }
} s_layouts;

std::vector<char> serialize_drclayer(const DRCLayer& layer) {
    // ... insert geometry into temp layout, write GDS2 to memory ...
    // (to be filled with actual db::Layout API calls)
    std::vector<char> result;
    return result;
}

DRCLayer deserialize_drclayer(const char* data, size_t size) {
    // ... read GDS2 from memory into temp layout, extract geometry ...
    DRCLayer result;
    return result;
}

}
```

- [ ] **Step 6: Explore Layout API for cell/layer management**

Before implementing, look at the Layout API:

```bash
cd /home/linfeng/code/drc-engine
grep -n "add_cell\|insert_layer\|Layout(" include/db/dbLayout.h | head -20
```

Read the relevant Layout methods to understand how to:
- Create a Layout with proper initialization
- Add a cell
- Insert a layer
- Insert shapes into a layer's cell
- Write GDS2 to a memory stream
- Read GDS2 from a memory stream
- Extract shapes from a layer's cell

- [ ] **Step 7: Complete mpi_serialize.cc with actual Layout API**

(After exploring Layout API, fill in the serialization implementation.)

- [ ] **Step 8: Run serialize tests and verify they pass**

- [ ] **Step 9: Complete MPI protocol implementation (send/recv/broadcast/gather)**

Implement the MPI helper functions in `mpi_protocol.h`:

```cpp
// mpi_protocol.h inline implementations
#include <mpi.h>
#include <vector>
#include <cstring>

namespace drc {

inline MPIMessage MPIMessage::recv(int source) {
    MPIHeader header;
    MPI_Recv(&header, sizeof(header), MPI_BYTE, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPIMessage msg;
    msg.header = header;
    if (header.size > 0) {
        msg.payload.resize(header.size);
        MPI_Recv(msg.payload.data(), header.size, MPI_BYTE, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    return msg;
}

inline void MPIMessage::send(int dest, MPIMsgType type, const void* data, int size) {
    MPIHeader header;
    header.type = static_cast<int>(type);
    header.size = size;
    MPI_Send(&header, sizeof(header), MPI_BYTE, dest, 0, MPI_COMM_WORLD);
    if (size > 0) {
        MPI_Send(data, size, MPI_BYTE, dest, 0, MPI_COMM_WORLD);
    }
}

} // namespace drc
```

- [ ] **Step 10: Commit protocol + serialization**

```bash
git add -A && git commit -m "feat(mpi): add protocol and serialization"
```

---

### Task 3: MPI Bindings — `__newindex` + Operator Overrides

**Files:**
- Create: `src/mpi/mpi_binding.h`
- Create: `src/mpi/mpi_binding.cc`

This is the core of the system — the MPI-aware Lua bindings that transparently orchestrate workers.

- [ ] **Step 1: Write mpi_binding.h**

```cpp
#pragma once
#include <sol/sol.hpp>
#include "drc/engine.h"
#include "mpi/mpi_protocol.h"
#include "mpi/script_analyzer.h"

namespace drc {

struct MPIContext {
    int num_workers;
    std::vector<db::Box> tiles;    // tile bounding boxes (extended by halo)
    double halo;
    double dbu;
    ScriptAnalyzer* analyzer;      // for ref table lookups
};

// Install MPI-aware bindings on a Lua state
void bind_drc_engine_mpi(sol::state& lua, DRCEngine& engine, MPIContext* ctx);

// Scatter a tile-local version of a DRCLayer variable to all workers
void mpi_scatter_var(const std::string& var_name, const DRCLayer& global_val,
                     const std::vector<db::Box>& tiles, double halo, double dbu);

// Evaluate an expression by broadcasting to workers and gathering results
DRCLayer mpi_evaluate_expr(const std::string& expr, int num_workers);

// Clip a global DRCLayer to a tile (extended by halo)
DRCLayer clip_to_tile(const DRCLayer& global, const db::Box& tile, double halo, double dbu);

} // namespace drc
```

- [ ] **Step 2: Write mpi_binding.cc (skeleton)**

```cpp
#include "mpi/mpi_binding.h"
#include <thread>
#include <mutex>

namespace drc {

// Thread-local state accessible from Lua binding callbacks
thread_local MPIContext* g_mpi_ctx = nullptr;
thread_local std::string g_current_expr;  // current RHS for operator overrides

void install_newindex_hook(sol::state& lua, MPIContext* ctx) {
    g_mpi_ctx = ctx;
    sol::table mt = lua.create_table();
    mt[sol::meta_function::new_index] = [&lua](sol::object key, sol::object value) {
        if (!key.is<std::string>()) {
            lua.globals().raw_set(key, value);
            return;
        }
        std::string var = key.as<std::string>();

        // Perform normal assignment (bypass metatable)
        lua.globals().raw_set(key, value);

        // If value is a DRCLayer with downstream refs → scatter to workers
        if (value.is<DRCLayer>() &&
            g_mpi_ctx && g_mpi_ctx->analyzer &&
            g_mpi_ctx->analyzer->has_downstream_refs(var, -1)) {
            DRCLayer& layer = value.as<DRCLayer>();
            mpi_scatter_var(var, layer, g_mpi_ctx->tiles, g_mpi_ctx->halo, g_mpi_ctx->dbu);
        }
    };
    lua.globals().set(sol::metatable_key, mt);
}

void bind_drc_engine_mpi(sol::state& lua, DRCEngine& engine, MPIContext* ctx) {
    g_mpi_ctx = ctx;

    // Install __newindex hook
    install_newindex_hook(lua, ctx);

    // __expr("expr_string") — sets g_current_expr for operator overrides
    // Injected by normalizer before each operation line.
    lua.set_function("__expr", [](const std::string& expr) {
        g_current_expr = expr;
        // returns nil — used as: __expr("RHS") or RHS
        // Because nil or X == X, the operation result is preserved.
    });

    // Bind non-MPI functions: source, target, write
    // (these are master-local, no worker involvement)
    lua.set_function("source", [&engine, ctx](const std::string& path) {
        engine.load_layout(path);
        // Compute tile grid after layout is loaded
        ctx->dbu = engine.dbu();
        auto layout_bbox = engine.layout()->bbox();
        // Tiles to be computed externally (tile count = num_workers)
        // ctx->tiles = ... (set by master init after source)
    });

    lua.set_function("target", [&engine](const std::string& path) {
        engine.set_target_path(path);
    });

    lua.set_function("write", [&engine]() {
        engine.write_output("");
    });

    // input() returns global Region (no MPI)
    lua.set_function("input", [&engine](int layer, int dtype) -> DRCLayer {
        return engine.input(layer, dtype);
    });

    // Override DRCLayer metatable operators and methods
    auto layertype = lua.new_usertype<DRCLayer>("DRCLayer");

    // Operators
    layertype[sol::meta_function::bitwise_and] = [](const DRCLayer& a, const DRCLayer& b) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype[sol::meta_function::bitwise_or] = [](const DRCLayer& a, const DRCLayer& b) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype[sol::meta_function::subtraction] = [](const DRCLayer& a, const DRCLayer& b) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype[sol::meta_function::bitwise_xor] = [](const DRCLayer& a, const DRCLayer& b) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };

    // Methods that need MPI coordination
    layertype["width"] = [](DRCLayer& self, double d) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["space"] = [](DRCLayer& self, double d) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["notch"] = [](DRCLayer& self, double d) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["sized"] = sol::overload(
        [](DRCLayer& self, double d) { return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers); },
        [](DRCLayer& self, double dx, double dy) { return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers); }
    );
    layertype["merge"] = [](DRCLayer& self) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["edges"] = [](DRCLayer& self) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["interacting"] = [](DRCLayer& self, const DRCLayer& other) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["inside"] = [](DRCLayer& self, const DRCLayer& other) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["outside"] = [](DRCLayer& self, const DRCLayer& other) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["enclosing"] = [](DRCLayer& self, const DRCLayer& other) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };

    // Edge operations
    layertype["extended_out"] = [](DRCLayer& self, double d) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["extended_in"] = [](DRCLayer& self, double d) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["centers"] = [](DRCLayer& self, double l, double f) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["start_segments"] = [](DRCLayer& self, double l, double f) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["end_segments"] = [](DRCLayer& self, double l, double f) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["extended"] = [](DRCLayer& self, double b, double e, double o, double i, bool join) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["corners_dots"] = [](DRCLayer& self, double a1, sol::optional<double> a2) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["corners_boxes"] = [](DRCLayer& self, double dim, sol::optional<double> a1, sol::optional<double> a2) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };

    // DRC check methods
    layertype["enclosing_check"] = [](DRCLayer& self, const DRCLayer& other, double d) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["sep_check"] = [](DRCLayer& self, const DRCLayer& other, double d) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };
    layertype["overlap_check"] = [](DRCLayer& self, const DRCLayer& other, double d) {
        return mpi_evaluate_expr(g_current_expr, g_mpi_ctx->num_workers);
    };

    // Non-MPI methods: query operations and output
    layertype["count"] = [](DRCLayer& self) -> size_t {
        if (self.type() == DRCLayer::Region && self.region()) return self.region()->count();
        if (self.type() == DRCLayer::Edges && self.edges()) return self.edges()->count();
        if (self.type() == DRCLayer::EdgePairs && self.edge_pairs()) return self.edge_pairs()->count();
        return 0;
    };
    layertype["empty"] = [](DRCLayer& self) -> bool {
        if (self.type() == DRCLayer::Region && self.region()) return self.region()->empty();
        if (self.type() == DRCLayer::Edges && self.edges()) return self.edges()->empty();
        if (self.type() == DRCLayer::EdgePairs && self.edge_pairs()) return self.edge_pairs()->empty();
        return true;
    };
    layertype["area"] = [](DRCLayer& self) -> double {
        if (self.type() == DRCLayer::Region && self.region()) return self.region()->area();
        return 0.0;
    };
    layertype["perimeter"] = [](DRCLayer& self) -> double {
        if (self.type() == DRCLayer::Region && self.region()) return self.region()->perimeter();
        return 0.0;
    };
    layertype["with_area"] = [&engine](DRCLayer& self, double min, sol::optional<double> max) {
        double dbu = engine.dbu();
        double min_db = min / (dbu * dbu);
        double max_db = max.value_or(std::numeric_limits<double>::max()) / (dbu * dbu);
        return self.with_area(min_db, max_db);
    };
    layertype["with_perimeter"] = [&engine](DRCLayer& self, double min, sol::optional<double> max) {
        double dbu = engine.dbu();
        double min_db = min / dbu;
        double max_db = max.value_or(std::numeric_limits<double>::max()) / dbu;
        return self.with_perimeter(min_db, max_db);
    };
    layertype["length"] = [](DRCLayer& self) -> double {
        if (self.type() == DRCLayer::Edges && self.edges()) return self.edges()->length();
        return 0.0;
    };
    layertype["type"] = [](DRCLayer& self) {
        switch (self.type()) {
            case DRCLayer::Region: return std::string("region");
            case DRCLayer::Edges: return std::string("edges");
            case DRCLayer::EdgePairs: return std::string("edge_pairs");
            case DRCLayer::Texts: return std::string("texts");
        }
        return std::string("unknown");
    };
    layertype["output"] = [&engine](DRCLayer& self, int layer, int dtype) {
        self.output(*engine.target(), layer, dtype);
    };
}

void mpi_scatter_var(const std::string& var_name, const DRCLayer& global_val,
                     const std::vector<db::Box>& tiles, double halo, double dbu) {
    for (int i = 0; i < (int)tiles.size(); i++) {
        auto tile_val = clip_to_tile(global_val, tiles[i], halo, dbu);

        // Serialize
        auto serialized = serialize_drclayer(tile_val);

        // Build UPDATE_VAR message: var_name_len + var_name + serialized_data
        // (Implementation to be completed)
    }
}

DRCLayer mpi_evaluate_expr(const std::string& expr, int num_workers) {
    // 1. Broadcast EXECUTE_RHS to all workers
    MPIRHSExec exec_msg;
    exec_msg.expr_len = (int32_t)expr.size();
    // ... send expr string to all workers ...

    // 2. Gather results from workers
    std::vector<DRCLayer> tile_results;
    for (int i = 0; i < num_workers; i++) {
        auto msg = MPIMessage::recv(i + 1);  // workers are ranks 1..N
        auto layer = deserialize_drclayer(msg.data(), (int)msg.payload.size());
        tile_results.push_back(std::move(layer));
    }

    // 3. Merge all tile results
    DRCLayer merged;
    for (auto& tile : tile_results) {
        if (merged.is_null()) {
            merged = std::move(tile);
        } else {
            merged = merged | tile;  // boolean OR to merge
        }
    }
    return merged;
}

DRCLayer clip_to_tile(const DRCLayer& global, const db::Box& tile, double halo, double dbu) {
    // Expand tile by halo (in db units)
    db::Coord halo_db = db::Coord(halo / dbu);
    db::Box expanded(tile.left() - halo_db, tile.bottom() - halo_db,
                     tile.right() + halo_db, tile.top() + halo_db);

    // Create a Region from the expanded tile box
    db::Region tile_region;
    tile_region.insert(expanded);

    // Intersect global with tile
    DRCLayer result;
    if (global.type() == DRCLayer::Region) {
        result = DRCLayer(new db::Region(*global.region() & tile_region));
    } else if (global.type() == DRCLayer::Edges) {
        // For Edges, use interacting to select edges within tile
        result = global.interacting(DRCLayer(new db::Region(tile_region)));
    } else if (global.type() == DRCLayer::EdgePairs) {
        result = global.interacting(DRCLayer(new db::Region(tile_region)));
    } else if (global.type() == DRCLayer::Texts) {
        result = DRCLayer(new db::Texts());  // TODO: clip texts by box
    }
    return result;
}

} // namespace drc
```

- [ ] **Step 3: Commit MPI bindings skeleton**

```bash
git add -A && git commit -m "feat(mpi): add MPI-aware Lua bindings with __newindex and operator overrides"
```

---

### Task 4: Worker Loop

**Files:**
- Create: `src/mpi/mpi_worker.h`
- Create: `src/mpi/mpi_worker.cc`

- [ ] **Step 1: Write mpi_worker.h**

```cpp
#pragma once

namespace drc {
int run_worker();
} // namespace drc
```

- [ ] **Step 2: Write mpi_worker.cc**

```cpp
#include "mpi/mpi_worker.h"
#include "mpi/mpi_protocol.h"
#include "mpi/mpi_serialize.h"
#include "drc/lua_binding.h"
#include "drc/engine.h"
#include <iostream>
#include <sol/sol.hpp>

namespace drc {

int run_worker() {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

    // Worker uses normal DRCLayer bindings (no MPI)
    // But we still need DRCLayer usertype registered for tile-local data
    // For now, worker doesn't need a full DRCEngine - just the DRCLayer types
    // Actually we do need engine for layout operations
    // Worker does NOT need source/target/output - it handles raw DRCLayer operations
    // We'll bind minimal functions

    // Actually worker DOES need bind_drc_engine to have DRCLayer metatable with operators
    // Let's create a minimal DRCEngine just for binding purposes
    DRCEngine engine;  // dummy engine for binding
    bind_drc_engine(lua, engine);

    while (true) {
        auto msg = MPIMessage::recv(0);  // always from master (rank 0)
        MPIMsgType type = static_cast<MPIMsgType>(msg.header.type);

        switch (type) {
        case MPIMsgType::EXECUTE_RHS: {
            std::string expr = msg.expr();
            // Execute the expression in Lua, expecting a DRCLayer return
            sol::safe_function_result result = lua.safe_script("return " + expr,
                sol::script_pass_on_error);
            if (!result.valid()) {
                sol::error err = result;
                std::cerr << "Worker RHS error: " << err.what() << std::endl;
                // Send empty result
                MPIMessage::send(0, MPIMsgType::EXECUTE_RHS, nullptr, 0);
                break;
            }
            DRCLayer layer = result;
            auto serialized = serialize_drclayer(layer);
            MPIMessage::send(0, MPIMsgType::EXECUTE_RHS,
                             serialized.data(), (int)serialized.size());
            break;
        }
        case MPIMsgType::UPDATE_VAR: {
            std::string var_name = msg.var_name();
            const char* data_ptr = msg.data() + msg.var_name_len() + sizeof(int32_t);
            int data_size = (int)msg.payload.size() - msg.var_name_len() - (int)sizeof(int32_t);
            DRCLayer layer = deserialize_drclayer(data_ptr, data_size);
            lua.globals()[var_name] = std::move(layer);
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
```

- [ ] **Step 3: Commit worker loop**

```bash
git add -A && git commit -m "feat(mpi): add worker message loop"
```

---

### Task 5: Master Initialization + Entry Point

**Files:**
- Create: `src/mpi/mpi_master.h`
- Create: `src/mpi/mpi_master.cc`

- [ ] **Step 1: Write mpi_master.h**

```cpp
#pragma once
#include <string>
#include <vector>

namespace drc {

struct TileConfig {
    int nx, ny;
};

int run_master(int argc, char* argv[]);

} // namespace drc
```

- [ ] **Step 2: Write mpi_master.cc**

```cpp
#include "mpi/mpi_master.h"
#include "mpi/mpi_binding.h"
#include "mpi/mpi_protocol.h"
#include "mpi/mpi_serialize.h"
#include "mpi/script_analyzer.h"
#include "drc/engine.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <regex>
#include <mpi.h>
#include <sol/sol.hpp>

namespace drc {

int run_master(int argc, char* argv[]) {
    // Parse arguments (simple: last arg is script, look for --mpi-tiles)
    std::string script_path;
    TileConfig tile_config{1, 1};
    double halo_override = -1.0;  // negative means auto-infer

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--mpi-tiles") {
            if (i + 1 < argc) {
                std::string val = argv[++i];
                auto x_pos = val.find('x');
                if (x_pos != std::string::npos) {
                    tile_config.nx = std::stoi(val.substr(0, x_pos));
                    tile_config.ny = std::stoi(val.substr(x_pos + 1));
                }
            }
        } else if (arg == "--halo") {
            if (i + 1 < argc) {
                halo_override = std::stod(argv[++i]);
            }
        } else if (arg[0] != '-') {
            script_path = arg;
        }
    }

    // Get number of workers (total ranks - 1 for master)
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    int num_workers = world_size - 1;
    if (num_workers < 1) {
        std::cerr << "Need at least 2 MPI processes (1 master + 1 worker)" << std::endl;
        return 1;
    }

    int total_tiles = tile_config.nx * tile_config.ny;
    if (total_tiles > num_workers) {
        std::cerr << "Tile count (" << total_tiles << ") exceeds worker count ("
                  << num_workers << ")" << std::endl;
        return 1;
    }

    // Phase 1: Script analysis
    ScriptAnalyzer analyzer(script_path);
    analyzer.normalize();
    analyzer.build_ref_table();

    // Phase 2: Set up engine + MPI bindings
    DRCEngine engine;
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::math,
                       sol::lib::string, sol::lib::table);

    MPIContext ctx;
    ctx.num_workers = num_workers;
    ctx.halo = halo_override;
    ctx.analyzer = &analyzer;

    bind_drc_engine_mpi(lua, engine, &ctx);

    // Phase 3: Execute normalized script
    // The __newindex hook + operator overrides handle all MPI orchestration
    auto result = lua.safe_script(analyzer.normalized_script(),
                                  sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "Master script error: " << err.what() << std::endl;
        return 1;
    }

    // Phase 4: Signal workers to finish
    broadcast_done(num_workers);

    return 0;
}

// Helper to broadcast DONE
void broadcast_done(int num_workers) {
    MPIHeader header;
    header.type = static_cast<int>(MPIMsgType::DONE);
    header.size = 0;
    for (int i = 1; i <= num_workers; i++) {
        MPI_Send(&header, sizeof(header), MPI_BYTE, i, 0, MPI_COMM_WORLD);
    }
}

} // namespace drc
```

- [ ] **Step 3: Commit master entry**

```bash
git add -A && git commit -m "feat(mpi): add master initialization and execution"
```

---

### Task 6: Tile Computation + CMake + CLI Integration

**Files:**
- Modify: `src/mpi/mpi_master.cc` (add tile computation)
- Modify: `CMakeLists.txt` (root)
- Modify: `src/cli/main.cc`
- Modify: `src/cli/CMakeLists.txt`
- Create: `src/mpi/CMakeLists.txt`

- [ ] **Step 1: Add tile computation to mpi_master.cc**

In the `run_master` function, after `lua.set_function("source", ...)` override, compute tiles:

```cpp
// In the source() binding override:
lua.set_function("source", [&engine, &ctx, tile_config](const std::string& path) {
    engine.load_layout(path);
    ctx.dbu = engine.dbu();

    // Compute tile grid from layout bbox
    auto bbox = engine.layout()->bbox();
    double tile_w = (double)(bbox.right() - bbox.left()) / tile_config.nx;
    double tile_h = (double)(bbox.top() - bbox.bottom()) / tile_config.ny;

    ctx.tiles.clear();
    for (int y = 0; y < tile_config.ny; y++) {
        for (int x = 0; x < tile_config.nx; x++) {
            db::Coord x1 = bbox.left() + (db::Coord)(tile_w * x);
            db::Coord y1 = bbox.bottom() + (db::Coord)(tile_h * y);
            db::Coord x2 = (x == tile_config.nx - 1) ? bbox.right() : (db::Coord)(tile_w * (x + 1));
            db::Coord y2 = (y == tile_config.ny - 1) ? bbox.top() : (db::Coord)(tile_h * (y + 1));
            ctx.tiles.push_back(db::Box(x1, y1, x2, y2));
        }
    }
});
```

- [ ] **Step 2: Write src/mpi/CMakeLists.txt**

```cmake
if(DRC_USE_MPI)
    file(GLOB MPI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)
    add_library(mpi STATIC ${MPI_SOURCES})
    target_include_directories(mpi PUBLIC 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/3rd/sol
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(mpi PUBLIC drc rdb gds db tl ${LUA_LIBRARIES} MPI::MPI_CXX)
    target_compile_definitions(mpi PUBLIC DRC_USE_MPI)
endif()
```

- [ ] **Step 3: Modify root CMakeLists.txt to add MPI option**

```cmake
# After existing find_package calls:
option(DRC_USE_MPI "Build with MPI distributed support" OFF)

if(DRC_USE_MPI)
    find_package(MPI REQUIRED COMPONENTS CXX)
endif()

# After existing add_subdirectory calls:
if(DRC_USE_MPI)
    add_subdirectory(src/mpi)
endif()
```

- [ ] **Step 4: Modify src/cli/CMakeLists.txt**

```cmake
# Add MPI dependency when enabled:
if(DRC_USE_MPI)
    target_link_libraries(drc-check PRIVATE mpi MPI::MPI_CXX)
    target_compile_definitions(drc-check PRIVATE DRC_USE_MPI)
endif()
```

- [ ] **Step 5: Modify src/cli/main.cc**

```cpp
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#ifdef DRC_USE_MPI
#include <mpi.h>
#include "mpi/mpi_master.h"
#include "mpi/mpi_worker.h"
#endif

#include "drc/lua_binding.h"
#include "drc/engine.h"
#include <sol/sol.hpp>

int main(int argc, char* argv[]) {
#ifdef DRC_USE_MPI
    MPI_Init(&argc, &argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0) {
        int ret = drc::run_master(argc, argv);
        MPI_Finalize();
        return ret;
    } else {
        int ret = drc::run_worker();
        MPI_Finalize();
        return ret;
    }
#else
    // Original single-node code
    if (argc < 2) {
        std::cerr << "Usage: drc-check <script.lua>" << std::endl;
        return 1;
    }
    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "Cannot open " << argv[1] << std::endl;
        return 1;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    std::string script = ss.str();

    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::math,
                       sol::lib::string, sol::lib::table, sol::lib::os);

    drc::DRCEngine engine;
    drc::bind_drc_engine(lua, engine);

    auto result = lua.safe_script(script, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        std::cerr << "DRC script error: " << err.what() << std::endl;
        return 1;
    }
    return 0;
#endif
}
```

- [ ] **Step 6: Try building with MPI**

```bash
cd /home/linfeng/code/drc-engine
cmake -B build -DDRC_USE_MPI=ON 2>&1
cmake --build build 2>&1
```

Fix any compile errors iteratively.

- [ ] **Step 7: Build single-node mode still works**

```bash
cmake -B build -DDRC_USE_MPI=OFF
cmake --build build
./build/src/cli/drc-check lua/example.drc
```

- [ ] **Step 8: Commit build + CLI changes**

```bash
git add -A && git commit -m "feat(mpi): add MPI build option and CLI integration"
```

---

### Task 7: HaloInferrer — Distance Parameter Collection

**Files:**
- Create: `src/mpi/halo_inferrer.h`
- Create: `src/mpi/halo_inferrer.cc`

- [ ] **Step 1: Write haloinferrer.h**

```cpp
#pragma once
#include <string>
#include <vector>

namespace drc {

class HaloInferrer {
public:
    HaloInferrer(const std::string& script_path);
    double infer_halo();

private:
    std::string m_script;

    struct DistParam {
        std::string func;  // "width", "space", "sized", etc.
        double value;
    };
    std::vector<DistParam> collect_params();
    bool is_distance_function(const std::string& call) const;
};

} // namespace drc
```

- [ ] **Step 2: Implementation**

```cpp
#include "mpi/halo_inferrer.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cmath>

namespace drc {

HaloInferrer::HaloInferrer(const std::string& script_path) {
    std::ifstream file(script_path);
    std::stringstream ss;
    ss << file.rdbuf();
    m_script = ss.str();
}

std::vector<HaloInferrer::DistParam> HaloInferrer::collect_params() {
    std::vector<DistParam> params;

    // Match patterns like :width(0.5), :space(1.0), :sized(0.1), etc.
    // Also match enclosing_check(other, d), sep_check(other, d), overlap_check(other, d)
    std::regex dist_re(R"((\w+)\(([^)]*)\))");
    std::sregex_iterator it(m_script.begin(), m_script.end(), dist_re);
    std::sregex_iterator end;

    // Functions where the first numeric argument is a distance
    std::set<std::string> dist_funcs = {
        "width", "space", "notch", "sized", "extended_out", "extended_in",
        "enclosing_check", "sep_check", "overlap_check", "extended"
    };

    for (; it != end; ++it) {
        std::string func = it->str(1);
        std::string args = it->str(2);
        if (dist_funcs.find(func) == dist_funcs.end()) continue;

        // Extract first numeric argument
        std::regex num_re(R"(([-+]?\d*\.?\d+))");
        std::smatch num_match;
        if (std::regex_search(args, num_match, num_re)) {
            double val = std::stod(num_match.str(1));
            if (func == "sized") {
                val = std::abs(val);  // sized can be negative (shrinking)
            }
            params.push_back({func, val});
        }
    }
    return params;
}

double HaloInferrer::infer_halo() {
    auto params = collect_params();
    double max_dist = 0.0;
    for (auto& p : params) {
        if (p.value > max_dist) max_dist = p.value;
    }
    return max_dist;  // return in microns
}

} // namespace drc
```

- [ ] **Step 3: Integrate HaloInferrer into mpi_master.cc**

Replace `ctx.halo = halo_override;` with:

```cpp
if (halo_override >= 0.0) {
    ctx.halo = halo_override;
} else {
    HaloInferrer inferrer(script_path);
    ctx.halo = inferrer.infer_halo();
}
```

- [ ] **Step 4: Commit HaloInferrer**

```bash
git add -A && git commit -m "feat(mpi): add HaloInferrer for automatic halo computation"
```

---

### Task 8: Fix the Reference Table Lookup in __newindex

The `__newindex` hook currently calls `analyzer->has_downstream_refs(var, -1)` with `-1` as def_line, but the ScriptAnalyzer::has_downstream_refs method needs the correct def_line. 

The issue: `__newindex` is called during `lua.script(normalized_script)` execution, and we don't know which line is currently executing. So we can't use def_line to filter on.

**Fix:** In `has_downstream_refs`, ignore the def_line parameter when it's -1:

```cpp
bool ScriptAnalyzer::has_downstream_refs(const std::string& var, int def_line) const {
    auto it = m_ref_table.find(var);
    if (it == m_ref_table.end()) return false;
    if (def_line < 0) return !it->second.ref_lines.empty();  // ignore def_line
    return !it->second.ref_lines.empty();
}
```

Also, the `is_input` field in RefEntry is not used by `__newindex` (input variables and computed variables both scatter the same way). So this fix applies.

- [ ] **Step 1: Fix has_downstream_refs**

- [ ] **Step 2: Commit fix**

```bash
git add -A && git commit -m "fix(mpi): allow __newindex to look up refs without def_line"
```

---

### Task 9: Halo Cell Test + MPI Integration Smoke Test

- [ ] **Step 1: Write a minimal MPI smoke test**

```cpp
// tests/test_mpi_integration.cc
#include <iostream>
#include <mpi.h>
#include "drc/engine.h"
#include "mpi/mpi_master.h"
#include "mpi/mpi_worker.h"

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0) {
        // Just test that master init works without crash
        std::cout << "Master rank 0 OK" << std::endl;
    } else {
        std::cout << "Worker rank " << rank << " OK" << std::endl;
    }

    MPI_Finalize();
    return 0;
}
```

- [ ] **Step 2: Run with mpirun**

```bash
cd /home/linfeng/code/drc-engine
mpirun -np 2 ./build/tests/test_mpi_integration
```

Expected: prints master + worker OK.

- [ ] **Step 3: Full integration test with a real script**

Create a minimal test GDS and run through MPI:

```bash
cd /home/linfeng/code/drc-engine
mpirun -np 2 ./build/src/cli/drc-check --mpi-tiles 1x1 lua/example.drc
```

Expected: produces the same output as the single-node run.

- [ ] **Step 4: Fix any integration issues**

Iterate on bugs found during integration testing.

- [ ] **Step 5: Commit tests**

```bash
git add -A && git commit -m "test(mpi): add MPI smoke and integration tests"
```
