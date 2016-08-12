#include "arg_config.hpp"
#include "yaml_config.hpp"
#include "handlers.hpp"
#include "converter.hpp"


int main(int argc, char** argv)
{
    using namespace xlsxconverter;

    if (argc < 2) {
        std::cerr << "command [yaml...]" << std::endl;
        return 1;
    }

    auto arg_config = ArgConfig(argc, argv);

    for (auto target: arg_config.targets) {
        auto yaml_path = arg_config.yaml_search_path + "/" + target;
        auto yaml_config = YamlConfig(target, arg_config);

        // solve relations
        for (auto rel: yaml_config.relations()) {
            if (handlers::RelationMap::has_cache(rel)) continue;
            auto rel_yaml_config = YamlConfig(rel.from, arg_config);
            auto relmap = handlers::RelationMap(rel, rel_yaml_config);
            Converter(rel_yaml_config).run(relmap);
            handlers::RelationMap::store_cache(std::move(relmap));
        }

        auto converter = Converter(yaml_config);
        if (yaml_config.handler.type == YamlConfig::Handler::Type::kJson) {;
            auto handler = handlers::JsonHandler(yaml_config);
            converter.run(handler);
        } else {
            throw utils::exception(yaml_config.name, ": unknown handler. type=", yaml_config.handler.type);
        }
    }

    return 0;
}

