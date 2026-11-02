#include "ConvertTool.h"
#include "Engine.h"

#include <iostream>
#include <string>

namespace {

bool expectEqual(const std::string& actual, const std::string& expected, const char* label) {
    if (actual == expected) {
        return true;
    }

    std::cerr << label << " failed: expected '" << expected
              << "', got '" << actual << "'\n";
    return false;
}

} // namespace

int main() {
    vKeyInit();

    convertToolFromCode = 0;
    convertToolToCode = 0;
    constexpr const char* sample = "tiếng việt";
    if (!expectEqual(convertUtil(sample), sample, "Unicode round-trip")) {
        return 1;
    }

    convertToolFromCode = 0;
    convertToolToCode = 2;
    const std::string vni = convertUtil(sample);
    if (vni.empty()) {
        std::cerr << "Unicode to VNI conversion returned an empty string\n";
        return 1;
    }

    convertToolFromCode = 2;
    convertToolToCode = 0;
    if (!expectEqual(convertUtil(vni), sample, "VNI round-trip")) {
        return 1;
    }

    std::cout << "Linux engine smoke tests passed\n";
    return 0;
}
