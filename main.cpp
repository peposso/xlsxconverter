#include "arg_config.hpp"
#include "yaml_config.hpp"
#include "converter.hpp"


int main(int argc, char** argv)
{
    using namespace std;
    using namespace xlsxconverter;

    if (argc < 2) {
        cerr << "command [yaml...]" << endl;
        return 1;
    }

    auto arg_config = ArgConfig(argc, argv);

    for (auto target: arg_config.targets) {
        auto yaml_path = arg_config.yaml_search_path + "/" + target;
        auto yaml_config = YamlConfig(yaml_path, arg_config);
        auto converter = Converter(yaml_config);
        converter.run();
    }

    return 0;
}

