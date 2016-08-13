#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <unordered_set>

#include <boost/optional.hpp>
#include <boost/any.hpp>
#include <yaml-cpp/yaml.h>

#include "utils.hpp"

namespace xlsxconverter {

struct YamlConfig
{
    struct Handler {
        enum Type {
            kNone, kJson, kCSV, kDjangoFixture,
        };
        Type type;
        std::string path;
        int indent = 4;
        bool sort_keys = false;
        bool allow_non_ascii = false;  // extension.

        inline Handler() {}
        inline Handler(YAML::Node node) {
            auto typepath = utils::split(node["type"].as<std::string>(), '.');
            auto typestr = typepath[typepath.size()-1];
            if      (typestr == "json") type = Type::kJson;
            else if (typestr == "csv") type = Type::kCSV;
            else if (typestr == "djangofixture") type = Type::kDjangoFixture;
            else throw utils::exception("unknown handler.type: ", typestr);
            if (auto n = node["path"]) path = n.as<std::string>();
            if (auto n = node["indent"]) indent = n.as<int>();
            if (auto n = node["sort_keys"]) sort_keys = n.as<bool>();
            if (auto n = node["allow_non_ascii"]) allow_non_ascii = n.as<bool>();
        }
    };
    struct Field {
        enum Type {
            kError, kInt, kFloat, kBool, kChar, kDateTime, kForeignKey,
        };
        struct Validate {
            bool unique = false;
            inline Validate(YAML::Node node) {
                if (auto n = node["unique"]) unique = n.as<bool>();
            }
        };
        struct Relation {
            std::string column;
            std::string from;
            std::string key;
            inline Relation(YAML::Node node) {
                if (auto n = node["column"]) column = n.as<std::string>();
                if (auto n = node["from"]) from = n.as<std::string>();
                if (auto n = node["key"]) key = n.as<std::string>();
            }
        };
        Type type;
        std::string column;
        std::string name;
        bool using_default = false;
        boost::any default_value;
        boost::optional<Validate> validate = boost::none;
        boost::optional<Relation> relation = boost::none;
        int index = -1;

        inline Field(YAML::Node node) {
            column = node["column"].as<std::string>();
            name = node["name"].as<std::string>();

            auto typestr = node["type"].as<std::string>();
            if      (typestr == "int") type = Type::kInt;
            else if (typestr == "float") type = Type::kFloat;
            else if (typestr == "bool") type = Type::kBool;
            else if (typestr == "char") type = Type::kChar;
            else if (typestr == "datetime") type = Type::kDateTime;
            else if (typestr == "foreignkey") type = Type::kForeignKey;
            else throw utils::exception("unknown field.type: ", typestr);

            if (auto n = node["default"]) {
                using_default = true;
                default_value = YamlConfig::node_to_any(n);
            }

            if (auto n = node["validate"]) {
                validate = Validate(n);
            }

            if (auto n = node["relation"]) {
                relation = Relation(n);
            }
        }
    };

    std::string name;
    std::string path;
    std::string target;
    std::string target_sheet_name;
    std::string target_xls_path;
    int row;
    Handler handler;
    std::vector<Field> fields;

    ArgConfig arg_config;

    inline
    YamlConfig(const std::string& path_, const ArgConfig& arg_config_)
    : path(path_), arg_config(arg_config_)
    {
        std::string fullpath = arg_config.yaml_search_path + "/" + path;
        if (!arg_config.quiet) {
            utils::log("target_yaml: ", path);
        }
        if (!utils::fexists(fullpath)) {
            throw utils::exception("yaml=", fullpath, " does not exist.");
        }
        auto doc = YAML::LoadFile(fullpath.c_str());

        // base.
        if (doc["name"]) {
            name = doc["name"].as<std::string>();
        } else {
            auto p = path.rfind('.');
            if (p != std::string::npos) {
                name = path.substr(0, p);
            } else {
                name = path;
            }
            for (auto& c: name) if (c == '/') c = '_';
        }
        target = doc["target"].as<std::string>();
        row = doc["row"].as<int>();
        if (target.substr(0, 7) == "xls:///") {
            target_xls_path = target.substr(7);
        } else {
            target_xls_path = target;
        }
        size_t pos;
        if ((pos = target_xls_path.find('#')) != std::string::npos) {
            target_sheet_name = target_xls_path.substr(pos+1);
            target_xls_path = target_xls_path.substr(0, pos);
        }

        // handler
        if (auto node = doc["handler"]) {;
            handler = Handler(node);
        }

        // fields
        fields.clear();
        for (auto node: doc["fields"]) {
            try {
                fields.push_back(Field(node));
            } catch (utils::exception& exc) {
                throw utils::exception(path, ": ", exc.what());
            }
        }

        if (handler.sort_keys) {
            std::sort(fields.begin(), fields.end(), [](Field& a, Field& b) {
                return a.column < b.column;
            });
        }

        int index = 0;
        for (auto& field: fields) {
            field.index = index++;
        }
    }

    inline
    std::vector<Field::Relation> relations() {
        auto vec = std::vector<Field::Relation>();
        for (auto& field: fields) {
            if (field.relation != boost::none) {
                vec.push_back(field.relation.value());
            }
        }
        return vec;
    }

    inline
    std::string get_xls_path() {
        return arg_config.xls_search_path + '/' + target_xls_path;
    }

    inline
    std::string get_output_path() {
        return arg_config.output_base_path + '/' + handler.path;
    }

    inline static
    boost::any node_to_any(YAML::Node node) {
        switch (node.Type()) {
            case YAML::NodeType::Undefined:
            case YAML::NodeType::Null: {
                return nullptr;
            }
            case YAML::NodeType::Sequence: {
                throw utils::exception("bad default type: sequence.");
            }
            case YAML::NodeType::Map: {
                throw utils::exception("bad default type: map.");
            }
            case YAML::NodeType::Scalar: {
                break;
            }
        }
        std::string strval = node.as<std::string>();
        if (strval.empty()) {
            return strval;
        }
        int64_t intval = 0;
        bool boolval = false;
        bool boolable = false;
        bool intable = false;
        try {
            intval = node.as<int64_t>();
            intable = true;
        } catch (const YAML::BadConversion& exc) {
            intable = false;
        }
        try {
            boolval = node.as<bool>();
            boolable = true;
        } catch (const YAML::BadConversion& exc) {
            boolable = false;
        }
        if (!intable && boolable) {
            // maybe. true, yes, false, no...
            return boolval;
        }
        if (strval.find('.') != std::string::npos) {
            try {
                return node.as<double>();
            } catch (const YAML::BadConversion& exc) {}
        }
        if (intable) {
            return intval;
        }
        return strval;
    }
};

}
