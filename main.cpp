#include "arg_config.hpp"
#include "yaml_config.hpp"
#include "handlers.hpp"
#include "converter.hpp"

#define EXCEPTION XLSXCONVERTER_UTILS_EXCEPTION

namespace {

void process(std::string target, xlsxconverter::ArgConfig& arg_config) {
    using namespace xlsxconverter;

    if (!arg_config.quiet) {
        utils::log("yaml: ", target);
    }
    auto yaml_config = YamlConfig(target, arg_config);

    // solve relations
    for (auto rel: yaml_config.relations()) {
        if (handlers::RelationMap::has_cache(rel)) continue;
        if (!arg_config.quiet) {
            utils::log("relation_yaml: ", rel.from);
        }
        auto rel_yaml_config = YamlConfig(rel.from, arg_config);
        auto relmap = handlers::RelationMap(rel, rel_yaml_config);
        Converter(rel_yaml_config).run(relmap);
        handlers::RelationMap::store_cache(std::move(relmap));
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

    try {
        for (auto target: arg_config->targets) {
            process(target, arg_config.value());
        }
    } catch (std::exception& exc) {
        std::cerr << "exception: " << exc.what() << std::endl;
        return 1;
    } 

    return 0;
}

