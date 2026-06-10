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
        std::string func;
        double value;
    };
    std::vector<DistParam> collect_params();
};

} // namespace drc
