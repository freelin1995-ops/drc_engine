#include <iostream>
#include <cassert>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include "mpi/script_analyzer.h"

static std::string write_temp_script(const std::string& content) {
    char path[] = "/tmp/drc_test_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) { perror("mkstemp"); exit(1); }
    auto bytes = write(fd, content.data(), content.size());
    (void)bytes;
    close(fd);
    return std::string(path);
}

static void test_chain_decomposition() {
    std::string path = write_temp_script(R"(
source("input.gds")
target("out.gds")
local w = input(10, 0):sized(0.1):width(0.5)
w:output(1, 0)
write()
)");
    drc::ScriptAnalyzer analyzer(path);
    analyzer.normalize();
    analyzer.build_ref_table();

    assert(analyzer.num_lines() > 3);
    std::string norm = analyzer.normalized_script();

    assert(norm.find("local") == std::string::npos);

    assert(norm.find("__t1 = input") != std::string::npos);

    assert(norm.find("__expr(") != std::string::npos);

    assert(analyzer.has_downstream_refs("__t1", -1));

    std::remove(path.c_str());
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

    std::string norm = analyzer.normalized_script();
    assert(norm.find("local") == std::string::npos);

    assert(analyzer.has_downstream_refs("a", -1));
    assert(analyzer.has_downstream_refs("merged", -1));

    std::remove(path.c_str());
    std::cout << "PASS: test_no_chain" << std::endl;
}

static void test_expr_injection() {
    std::string path = write_temp_script(R"(
source("x.gds")
target("y.gds")
local a = input(10, 0)
local merged = a | b
write()
)");
    drc::ScriptAnalyzer analyzer(path);
    analyzer.normalize();

    std::string norm = analyzer.normalized_script();
    assert(norm.find("__expr(\"a | b\")") != std::string::npos);
    assert(norm.find("__expr(\"input") == std::string::npos);

    std::remove(path.c_str());
    std::cout << "PASS: test_expr_injection" << std::endl;
}

int main() {
    test_chain_decomposition();
    test_no_chain();
    test_expr_injection();
    std::cout << "All script_analyzer tests passed!" << std::endl;
    return 0;
}
