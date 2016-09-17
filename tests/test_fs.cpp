#include "utils.hpp"

int main(int argc, char** argv) {
    using namespace xlsxconverter;
    using namespace xlsxconverter::utils;

    utils::log("iterdir");
    for (auto& e: fs::iterdir(".")) {
        utils::log("name=", e.name);
    }

    utils::log("walk");
    for (auto& e: fs::walk("external", "*.h")) {
        utils::log("name=", e);
    }

    return 0;
}
