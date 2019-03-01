#include "arg_config.hpp"
#include "yaml_config.hpp"
#include "handlers.hpp"
#include "converter.hpp"

namespace {
void test() {
    using namespace xlsxconverter;
    if (0) {
        int argc;
        char** argv;
        auto arg_config = ArgConfig(argc, argv);
        auto yaml_config = YamlConfig("", arg_config);
        auto yaml_handler = yaml_config.handlers[0];
        auto relmap = handlers::RelationMap(yaml_config.fields[0].relation.value(), yaml_config);
        auto cnv = Converter(yaml_config, true);
        cnv.run(relmap);
        handlers::RelationMap::store_cache(std::move(relmap));
        {
            auto h = handlers::JsonHandler(yaml_handler, yaml_config);
            cnv.run(h);
        }
        {
            auto h = handlers::DjangoFixtureHandler(yaml_handler, yaml_config);
            cnv.run(h);
        }
        {
            auto h = handlers::CSVHandler(yaml_handler, yaml_config);
            cnv.run(h);
        }
        {
            auto h = handlers::LuaHandler(yaml_handler, yaml_config);
            cnv.run(h);
        }
    }
}
}

#ifdef MAIN
int main(int argc, char** argv) { test(); return 0; }
#endif
