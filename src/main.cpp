// Copyright (c) 2016 peposso All Rights Reserved.
// Released under the MIT license
#include <string>
#include <vector>
#include <utility>
#include <functional>
#include <thread>
#include <mutex>

#include "arg_config.hpp"
#include "yaml_config.hpp"
#include "handlers.hpp"
#include "converter.hpp"

#define EXCEPTION XLSXCONVERTER_UTILS_EXCEPTION

namespace {

using ArgConfig = xlsxconverter::ArgConfig;
using YamlConfig = xlsxconverter::YamlConfig;
using Converter = xlsxconverter::Converter;
namespace utils = xlsxconverter::utils;
namespace handlers = xlsxconverter::handlers;

struct MainTask {
    struct RelationYaml {
        std::string id;
        YamlConfig yaml_config;
        YamlConfig::Field::Relation relation;
        inline
        RelationYaml(std::string id, YamlConfig yaml_config, YamlConfig::Field::Relation relation)
            : yaml_config(yaml_config),
              relation(relation)
        {}
    };
    using lock_guard = std::lock_guard<std::mutex>;
    utils::mutex_list<std::string> targets;
    utils::mutex_list<YamlConfig> yaml_configs;
    utils::mutex_list<YamlConfig::Field::Relation> relations;
    utils::mutex_list<RelationYaml> relation_yamls;
    utils::mutex_map<std::string, int> target_xls_counts;

    ArgConfig& arg_config;
    bool canceled;
    std::mutex phase1_done;
    std::mutex phase2_done;
    std::mutex phase3_done;
    std::atomic_int phase1_running;
    std::atomic_int phase2_running;
    std::atomic_int phase3_running;

    MainTask(ArgConfig& arg_config, int jobs)
            : canceled(false),
              arg_config(arg_config),
              targets(),
              yaml_configs(),
              relations(),
              relation_yamls() {
        if (arg_config.targets.empty() && !arg_config.yaml_search_paths.empty()) {
            for (auto& target : arg_config.search_yaml_target_all()) {
                targets.push_back(target);
            }
        } else {
            for (auto& target : arg_config.targets) {
                targets.push_back(target);
            }
        }
        phase1_done.lock();
        phase2_done.lock();
        phase3_done.lock();
        phase1_running = jobs;
        phase2_running = jobs;
        phase3_running = jobs;

        utils::logging_lock();
    }

    void run() {
        // yaml
        phase1();
        if (canceled) return;
        { lock_guard lock(phase1_done); }

        // relation
        phase2();
        if (canceled) return;
        { lock_guard lock(phase2_done); }

        // relation mapping
        phase3();
        if (canceled) return;
        { lock_guard lock(phase3_done); }

        // convert
        phase4();
    }

    struct id_functor {
        std::string id;
        inline explicit id_functor(std::string id) : id(id) {}
        template<class T> bool operator()(T& t) { return t.id == id; }
    };

    void phase1() {
        while (!canceled) {
            auto target_opt = targets.move_front();
            if (target_opt == boost::none) break;
            auto target = target_opt.value();

            try {
                auto yaml_config = YamlConfig(target, arg_config);
                for (auto rel : yaml_config.relations()) {
                    // check file existance.
                    arg_config.search_yaml_path(rel.from);
                    if (!relations.any(id_functor(rel.id))) {
                        relations.push_back(std::move(rel));
                    }
                }
                auto paths = yaml_config.get_xls_paths();
                for (auto path : paths) {
                    target_xls_counts.add(path, 1);
                }
                yaml_configs.push_back(std::move(yaml_config));
            } catch (std::exception& exc) {
                throw EXCEPTION(target, ": relation error: ", exc.what());
            }
        }
        --phase1_running;
        if (phase1_running.load() == 0) {
            if (relations.empty()) {
                target_xls_counts.erase([](std::string, int c){return c == 1;});
                phase3_done.unlock();
                phase2_done.unlock();
            }
            phase1_done.unlock();
        }
    }
    void phase2() {
        while (!canceled) {
            auto relation = relations.move_front();
            if (relation == boost::none) break;

            try {
                auto yaml_config = YamlConfig(relation->from, arg_config);
                auto paths = yaml_config.get_xls_paths();
                for (auto path : paths) {
                    target_xls_counts.add(path, 1);
                }
                relation_yamls.push_back(RelationYaml(relation->id, std::move(yaml_config),
                                                      std::move(relation.value())));
            } catch (std::exception& exc) {
                throw EXCEPTION(relation->from, ": ", exc.what());
            }
        }
        --phase2_running;
        if (phase2_running.load() == 0) {
            target_xls_counts.erase([](std::string, int c) { return c == 1; });
            phase2_done.unlock();
        }
    }
    void phase3() {
        while (!canceled) {
            auto rel_yaml = relation_yamls.move_back();
            if (rel_yaml == boost::none) break;

            auto rel = std::move(rel_yaml->relation);
            auto yaml_config = std::move(rel_yaml->yaml_config);

            if (handlers::RelationMap::has_cache(rel)) {
                continue;
            }
            auto relmap = handlers::RelationMap(rel, yaml_config);
            auto using_shared = target_xls_counts.has(yaml_config.get_xls_paths()[0]);
            Converter(yaml_config, using_shared, true).run(relmap);
            handlers::RelationMap::store_cache(std::move(relmap));
        }
        --phase3_running;
        if (phase3_running.load() == 0) {
            phase3_done.unlock();
        }
    }
    void phase4() {
        while (!canceled) {
            auto yaml_config_opt = yaml_configs.move_front();
            if (yaml_config_opt == boost::none) break;
            auto& yaml_config = yaml_config_opt.value();

            using HT = YamlConfig::Handler::Type;
            auto using_shared = target_xls_counts.has(yaml_config.get_xls_paths()[0]);
            auto converter = Converter(yaml_config, using_shared);
            for (auto& yaml_handler : yaml_config.handlers) {
                if (yaml_handler.type == YamlConfig::Handler::Type::kNone) {
                    if (!arg_config.quiet) {
                        utils::log("skip: ", yaml_config.path);
                    }
                    continue;
                }
                // utils::log("conv: ", yaml_config.path);
                switch (yaml_handler.type) {;
                    #define CASE(i, T) \
                        case i: { \
                            auto handler = T(yaml_handler, yaml_config); \
                            converter.run(handler); \
                            if (canceled) break; \
                            handler.save(arg_config); \
                            break; \
                        }
                    CASE(HT::kJson, handlers::JsonHandler);
                    CASE(HT::kDjangoFixture, handlers::DjangoFixtureHandler);
                    CASE(HT::kCSV, handlers::CSVHandler);
                    CASE(HT::kLua, handlers::LuaHandler);
                    CASE(HT::kTemplate, handlers::TemplateHandler);
                    CASE(HT::kMessagePack, handlers::MessagePackHandler);
                    #undef CASE
                    default: {
                        throw EXCEPTION(yaml_config.path,
                                        ": handler.type=", yaml_handler.type_name,
                                        ": not implemented.");
                    }
                }
            }
        }
    }
};

}  // anonymous namespace

int main(int argc, char** argv) {
    using ArgConfig = xlsxconverter::ArgConfig;

    boost::optional<ArgConfig> arg_config;
    try {
        arg_config = ArgConfig(argc, argv);
    } catch (std::exception& exc) {
        std::cerr << exc.what() << std::endl << std::endl;
        std::cerr << ArgConfig::help() << std::endl;
        return 1;
    }

    if (arg_config->targets.empty() && arg_config->yaml_search_paths.empty()) {
        std::cerr << ArgConfig::help() << std::endl;
        return 1;
    }

    int jobs = arg_config->jobs;
    if (!arg_config->quiet) {
        utils::log("jobs: ", jobs);
    }

    MainTask task(arg_config.value(), jobs);

    // if (jobs > task.targets.size()) {
    //     jobs = task.targets.size();
    // }

    auto work = std::function<void(void)>([&]() {
        #ifndef DEBUG
        try {
        #endif
            task.run();
        #ifndef DEBUG
        } catch (std::exception& exc) {
            utils::logerr("exception: ", exc.what());
            task.canceled = true;
            task.phase1_done.unlock();
            task.phase2_done.unlock();
            task.phase3_done.unlock();
            return;
        }
        #endif
    });

    auto tasks = std::vector<std::thread>();
    for (int i = 0; i < jobs - 1; ++i) {
        tasks.emplace_back(work);
    }

    // 1 task run in current thread.
    work();

    for (int i = 0; i < jobs - 1; ++i) {
        tasks[i].join();
    }

    if (task.canceled) {
        return 1;
    }
    return 0;
}

