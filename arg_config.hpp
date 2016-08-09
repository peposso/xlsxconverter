#pragma once
#include <string>
#include <vector>
#include "util.hpp"

namespace xlsxconverter {

struct ArgConfig
{
    std::string target;
    std::string xls_search_path;
    std::string yaml_search_path;
    std::string output_base_path;
    std::vector<std::string> targets;

    inline
    ArgConfig(int argc, char** argv)
    {
        xls_search_path = ".";
        yaml_search_path = ".";
        output_base_path = ".";

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg.substr(0, 2) == "--") {;
                if (i >= argc - 1) {
                    throw std::runtime_error("failed argparse.");
                }
                if (arg == "--xls_search_path") {
                    xls_search_path = argv[++i];
                    continue;
                } else if (arg == "--yaml_search_path") {
                    yaml_search_path = argv[++i];
                    continue;
                } else if (arg == "--output_base_path") {
                    output_base_path = argv[++i];
                    continue;
                } else {
                    throw util::exception("failed argparse. arg=[%s]", arg);
                }
            } else {
                targets.push_back(arg);
            }
        }
    }
};

}
