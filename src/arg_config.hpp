// Copyright (c) 2016 peposso All Rights Reserved.
// Released under the MIT license
#pragma once
#include <string>
#include <vector>
#include <thread>
#include <algorithm>

#include "utils.hpp"

#ifndef BUILD_REVISION
#define BUILD_REVISION ""
#endif
#define EXCEPTION XLSXCONVERTER_UTILS_EXCEPTION

namespace xlsxconverter {

struct ArgConfig {
    std::string name;
    std::string target;
    std::string xls_search_path;
    std::vector<std::string> yaml_search_paths;
    std::string output_base_path;
    bool quiet;
    bool verbose;
    bool no_cache;
    int tz_seconds;
    int jobs;
    std::vector<std::string> targets;

    std::vector<std::string> args;

    inline
    ArgConfig(int argc, char** argv)
            : xls_search_path("."),
              yaml_search_paths(),
              output_base_path("."),
              quiet(false),
              no_cache(false),
              tz_seconds(utils::dateutil::local_tz_seconds()),
              jobs(std::thread::hardware_concurrency()) {
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
                    yaml_search_paths = utils::split(*++it, ',');
                    continue;
                } else if (arg == "--output_base_path" && !last) {
                    output_base_path = *++it;
                    continue;
                } else if (arg == "--jobs" && !last) {
                    auto s = *++it;
                    if (s == "full") {
                        jobs = std::thread::hardware_concurrency();
                    } else if (s == "half") {
                        jobs = std::thread::hardware_concurrency() / 2;
                    } else if (s == "quarter") {
                        jobs = std::thread::hardware_concurrency() / 4;
                    } else {
                        jobs = std::stoi(s);
                    }
                    jobs = jobs < 1 ? 1 : jobs;
                    jobs = jobs > 20 ? 20 : jobs;
                    continue;
                } else if (arg == "--quiet") {
                    quiet = true;
                    continue;
                } else if (arg == "--verbose") {
                    verbose = true;
                    continue;
                } else if (arg == "--no_cache") {
                    no_cache = true;
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
            "xlsxconverter (rev."  << BUILD_REVISION << ")" << std::endl <<
            usage  << " [--quiet]" << std::endl <<
            indent << " [--no_cache]" << std::endl <<
            indent << " [--jobs <'full'|'half'|'quarter'|int>]" << std::endl <<
            indent << " [--xls_search_path <path>]" << std::endl <<
            indent << " [--yaml_search_path <paths>]" << std::endl <<
            indent << " [--output_base_path <path>]" << std::endl <<
            indent << " [--timezone <tz>]" << std::endl <<
            indent << " [<target_yaml> ...]" << std::endl <<
            "";

        return ss.str();
    }

    inline
    std::string search_yaml_path(const std::string& name) {
        if (yaml_search_paths.empty()) {
            // from cwd
            if (utils::fs::exists(name)) return name;
            throw EXCEPTION(name, ": does not exist.");
        }
        for (std::string& dir : yaml_search_paths) {
            auto path = utils::fs::joinpath(dir, name);
            if (utils::fs::exists(path)) return path;
        }
        throw EXCEPTION(name, ": does not exist.");
    }

    inline
    std::vector<std::string> search_yaml_target_all() {
        if (yaml_search_paths.empty()) {
            throw EXCEPTION("requires --yaml_search_path.");
        }
        std::vector<std::string> r;
        if (!yaml_search_paths.empty()) {
            auto dir = yaml_search_paths[0];
            for (std::string& name : utils::fs::walk(dir, "*.yaml")) {
                r.push_back(name);
            }
            for (std::string& name : utils::fs::walk(dir, "*.yml")) {
                r.push_back(name);
            }
        }
        if (r.empty()) {
            throw EXCEPTION("no yaml files.");
        }
        std::sort(r.begin(), r.end());
        return r;
    }
};

}  // namespace xlsxconverter
#undef EXCEPTION
