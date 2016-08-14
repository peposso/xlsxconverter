#include <functional>
#include <thread>
#include <mutex>

#include "arg_config.hpp"
#include "yaml_config.hpp"
#include "handlers.hpp"
#include "converter.hpp"

#define EXCEPTION XLSXCONVERTER_UTILS_EXCEPTION

namespace {

std::mutex relation_mutex_map_mutex;
std::unordered_map<std::string, std::mutex> relation_mutex_map;

void process(std::string target, xlsxconverter::ArgConfig& arg_config) {
    using namespace xlsxconverter;

    if (!arg_config.quiet) {
        utils::log("yaml: ", target);
    }
    auto yaml_config = YamlConfig(target, arg_config);

    // solve relations
    auto relations = yaml_config.relations();
    if (!relations.empty()) {
        for (auto rel: relations) {
            if (handlers::RelationMap::has_cache(rel)) continue;
            auto key = rel.column + ':' + rel.from + ':' + rel.key;
            boost::optional<std::mutex&> mtx;
            {
                std::lock_guard<std::mutex> lock(relation_mutex_map_mutex);
                mtx = relation_mutex_map[key];
            }
            std::lock_guard<std::mutex> lock(mtx.value());
            if (handlers::RelationMap::has_cache(rel)) continue;
            if (!arg_config.quiet) {
                utils::log("relation_yaml: ", rel.from);
            }
            auto rel_yaml_config = YamlConfig(rel.from, arg_config);
            auto relmap = handlers::RelationMap(rel, rel_yaml_config);
            Converter(rel_yaml_config).run(relmap);
            handlers::RelationMap::store_cache(std::move(relmap));
        }
    }

    auto converter = Converter(yaml_config);
    switch (yaml_config.handler.type) {
        case YamlConfig::Handler::Type::kJson: {
            auto handler = handlers::JsonHandler(yaml_config);
            converter.run(handler);
            break;
        }
        case YamlConfig::Handler::Type::kDjangoFixture: {
            auto handler = handlers::DjangoFixtureHandler(yaml_config);
            converter.run(handler);
            break;
        }
        case YamlConfig::Handler::Type::kCSV: {
            auto handler = handlers::CSVHandler(yaml_config);
            converter.run(handler);
            break;
        }
        case YamlConfig::Handler::Type::kLua: {
            auto handler = handlers::LuaHandler(yaml_config);
            converter.run(handler);
            break;
        }
        default: {
            throw EXCEPTION(yaml_config.name, ": handler.type=", yaml_config.handler.type_name,
                            ": not implemented.");
        }
    }
}

}  // namespace anonymous

int main(int argc, char** argv)
{
    using namespace xlsxconverter;

    boost::optional<ArgConfig> arg_config;
    try {
        arg_config = ArgConfig(argc, argv);
    } catch (std::exception& exc) {
        std::cerr << exc.what() << std::endl << std::endl;
        std::cerr << ArgConfig::help() << std::endl;
        return 1;
    }

    if (arg_config->targets.size() == 0) {
        std::cerr << ArgConfig::help() << std::endl;
        return 1;
    }

    std::list<std::string> targets;
    for (auto& target: arg_config->targets) {
        targets.push_back(target);
    }

    int jobs = arg_config->jobs;
    if (!arg_config->quiet) {
        utils::log("jobs: ", jobs);
    }
    if (jobs > targets.size()) {
        jobs = targets.size();
    }

    auto& arg_config_ref = arg_config.value();
    std::mutex target_mtx;
    bool canceled;

    auto work = std::function<void(void)>([&]() {
        while (!canceled) {
            std::string target;
            {
                std::lock_guard<std::mutex> lock(target_mtx);
                if (targets.empty()) {
                    return;
                }
                target = targets.front();
                targets.pop_front();
            }
            try {
                if (canceled) return;
                process(target, arg_config_ref);
            } catch (std::exception& exc) {
                std::cerr << "exception: " << exc.what() << std::endl;
                canceled = true;
                return;
            }
        }
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

    if (canceled) {
        return 1;
    }
    return 0;
}

