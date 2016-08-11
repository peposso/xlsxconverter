#pragma once
#include <string>
#include <vector>
#include "utils.hpp"

namespace xlsxconverter {

struct ArgConfig
{
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
                } else if (arg == "--timezone") {
                    auto s = *++it;
                    bool ok; int h, m; size_t p;
                    std::tie(ok, h, m, p) = utils::dateutil::parse_timezone(s);
                    if (!ok) {
                        throw utils::exception("failed argparse: timezone=[%s]", s);
                    }
                    tz_seconds = (h * 60 + m) * 60;
                    continue;
                } else {
                    throw utils::exception("failed argparse. arg=[%s]", arg);
                }
            } else {
                targets.push_back(arg);
            }
        }
    }
};

}
