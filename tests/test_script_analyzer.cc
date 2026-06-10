#include <iostream>
#include <cassert>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include "mpi/script_analyzer.h"

struct TempScript {
    std::string path;
    TempScript(const std::string& content) {
        char tmpl[] = "/tmp/drc_test_XXXXXX";
        int fd = mkstemp(tmpl);
        write(fd, content.data(), content.size());
        close(fd);
        path = tmpl;
    }
    ~TempScript() { std::remove(path.c_str()); }
};

static void test_chain_decomposition() {
    TempScript s(R"(
source("input.gds")
target("out.gds")
local w = input(10, 0):sized(0.1):width(0.5)
w:output(1, 0)
write()
)");
    drc::ScriptAnalyzer analyzer(s.path);
    assert(analyzer.valid());
    analyzer.normalize();
    analyzer.build_ref_table();

    assert(analyzer.num_lines() > 3);
    std::string norm = analyzer.normalized_script();

    assert(norm.find("local") == std::string::npos);

    assert(norm.find("__t1 = input") != std::string::npos);

    assert(norm.find("__expr(") != std::string::npos);

    assert(analyzer.has_downstream_refs("__t1", -1));

    std::cout << "PASS: test_chain_decomposition" << std::endl;
}

static void test_no_chain() {
    TempScript s(R"(
source("a.gds")
target("b.gds")
local a = input(10, 0)
local b = input(30, 0)
local merged = a | b
local w = merged:width(0.5)
w:output(1, 0)
write()
)");
    drc::ScriptAnalyzer analyzer(s.path);
    assert(analyzer.valid());
    analyzer.normalize();
    analyzer.build_ref_table();

    std::string norm = analyzer.normalized_script();
    assert(norm.find("local") == std::string::npos);

    assert(analyzer.has_downstream_refs("a", -1));
    assert(analyzer.has_downstream_refs("merged", -1));

    std::cout << "PASS: test_no_chain" << std::endl;
}

static void test_expr_injection() {
    TempScript s(R"(
source("x.gds")
target("y.gds")
local a = input(10, 0)
local merged = a | b
write()
)");
    drc::ScriptAnalyzer analyzer(s.path);
    assert(analyzer.valid());
    analyzer.normalize();

    std::string norm = analyzer.normalized_script();
    assert(norm.find("__expr(\"a | b\")") != std::string::npos);
    assert(norm.find("__expr(\"input") == std::string::npos);

    std::cout << "PASS: test_expr_injection" << std::endl;
}

int main() {
    test_chain_decomposition();
    test_no_chain();
    test_expr_injection();
    std::cout << "All script_analyzer tests passed!" << std::endl;
    return 0;
}
