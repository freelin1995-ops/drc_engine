#include <iostream>
#include <cassert>
#include "drc/engine.h"

int main() {
    // Test DRCEngine creation
    drc::DRCEngine engine;
    std::cout << "DRCEngine created OK" << std::endl;

    // Test DRCLayer creation and boolean ops
    {
        auto r1 = drc::DRCLayer(new db::Region());
        auto r2 = drc::DRCLayer(new db::Region());
        auto r3 = r1 & r2;
        auto r4 = r1 | r2;
        auto r5 = r1 - r2;
        std::cout << "Boolean ops OK" << std::endl;
    }

    // Test merge
    {
        auto r1 = drc::DRCLayer(new db::Region());
        auto m = r1.merge();
        std::cout << "Merge OK" << std::endl;
    }

    std::cout << "All basic tests passed!" << std::endl;
    return 0;
}
