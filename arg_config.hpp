#pragma once
#include <string>
#include <vector>
#include "utils.hpp"

#define EXCEPTION XLSXCONVERTER_UTILS_EXCEPTION

namespace xlsxconverter {

struct ArgConfig
{
    std::string name;
    std::string target;
    std::string xls_search_path;
    std::string yaml_search_path;
    std::string output_base_path;
    bool quiet;
    int tz_seconds;
    std::vector<std::string> targets;

    std::vector<std::string> args;

    inline
    ArgConfig(int argc, char** argv)
        : xls_search_path("."),
          yaml_search_path("."),
          output_base_path("."),
          quiet(false),
          tz_seconds(utils::dateutil::local_tz_seconds())
    {
        name = argc > 0 ? argv[0] : "";
        for (int i = 1; i < argc; ++i) {
            args.push_back(argv[i]);
        }


        auto it = args.begin();
        for (auto it = args.begin(); it != args.end(); ++it) {
            auto arg = *it;
            auto last = it+1 == args.end();
            if (arg.substr(0, 2) == "--") {;
                if (arg == "--xls_search_path" && !last) {
                    xls_search_path = *++it;
                    continue;
                } else if (arg == "--yaml_search_path" && !last) {
                    yaml_search_path = *++it;
                    continue;
                } else if (arg == "--output_base_path" && !last) {
                    output_base_path = *++it;
                    continue;
                } else if (arg == "--quiet") {
                    quiet = true;
                    continue;
                } else if (arg == "--timezone" && !last) {
                    auto s = *++it;
                    bool ok; int h, m; size_t p;
                    std::tie(ok, h, m, p) = utils::dateutil::parse_timezone(s);
                    if (!ok) {
                        throw EXCEPTION("arg=", s, ": failed date parse.");
                    }
                    tz_seconds = (h * 60 + m) * 60;
                    continue;
                } else {
                    throw EXCEPTION("arg=", arg, ": failed argparse.");
                }
            } else {
                targets.push_back(arg);
            }
        }
    }

    inline static
    std::string help() {
        std::string usage = "usage: xlsxconverter";
        auto indent = std::string(usage.size(), ' ');

        auto ss = std::stringstream();
        ss <<
            usage  << " [--quiet]" << std::endl <<
            indent << " [--xls_search_path <path>]" << std::endl <<
            indent << " [--yaml_search_path <path>]" << std::endl <<
            indent << " [--output_base_path <path>]" << std::endl <<
            indent << " [--timezone <tz>]" << std::endl <<
            indent << " <target_yaml> [<target_yaml> ...]" << std::endl <<
            "";

        return ss.str();
    }
};

}
#undef EXCEPTION
