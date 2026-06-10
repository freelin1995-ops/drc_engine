#include "mpi/halo_inferrer.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cmath>
#include <set>

namespace drc {

HaloInferrer::HaloInferrer(const std::string& script_path) {
    std::ifstream file(script_path);
    std::stringstream ss;
    ss << file.rdbuf();
    m_script = ss.str();
}

std::vector<HaloInferrer::DistParam> HaloInferrer::collect_params() {
    std::vector<DistParam> params;
    std::regex dist_re(R"((\w+)\(([^)]*)\))");
    std::sregex_iterator it(m_script.begin(), m_script.end(), dist_re);
    std::sregex_iterator end;

    std::set<std::string> dist_funcs = {
        "width", "space", "notch", "sized", "extended_out", "extended_in",
        "enclosing_check", "sep_check", "overlap_check", "extended"
    };

    for (; it != end; ++it) {
        std::string func = it->str(1);
        std::string args = it->str(2);
        if (dist_funcs.find(func) == dist_funcs.end()) continue;

        std::regex num_re(R"(([-+]?\d*\.?\d+))");
        std::smatch num_match;
        if (std::regex_search(args, num_match, num_re)) {
            double val = std::stod(num_match.str(1));
            if (func == "sized") val = std::abs(val);
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
    return max_dist;
}

} // namespace drc
