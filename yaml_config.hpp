#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>

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

        void parse_type(std::string typestr) {
            if      (typestr == "json") { this->type = Type::kJson; }
            else if (typestr == "csv") { this->type = Type::kCSV; }
            else if (typestr == "hadler.djangofixture") { this->type = Type::kDjangoFixture; }
        }
    };
    struct Field {
        enum Type {
            kError, kInt, kFloat, kBool, kChar, kDateTime, kForeignKey,
        };
        struct Validate {
            bool unique = false;
        };
        Type type;
        std::string column;
        std::string name;
        bool using_default = false;
        boost::any default_value;

        void parse_type(std::string typestr) {
            if      (typestr == "int") { type = Type::kInt; }
            else if (typestr == "float") { type = Type::kFloat; }
            else if (typestr == "bool") { type = Type::kBool; }
            else if (typestr == "char") { type = Type::kChar; }
            else if (typestr == "datetime") { type = Type::kDateTime; }
            else if (typestr == "foreignkey") { type = Type::kForeignKey; }
        }
        void parse_default(YAML::Node node, const std::string& path) {
            if (!node.IsDefined()) {
                using_default = false;
                return;
            }
            using_default = true;
            default_value = YamlConfig::node_to_any(node);
            if (default_value.type() == typeid(utils::exception)) {
                throw utils::exception(
                    "%s: field=%s: bad default type.",
                    path, column);
            }
        }
    };

    std::string target;
    std::string target_sheet_name;
    std::string target_xls_path;
    int row;
    Handler handler;
    std::vector<Field> fields;
    ArgConfig arg_config;

    inline
    YamlConfig(const std::string& path, const ArgConfig& arg_config_)
    : handler(), arg_config(arg_config_)
    {
        if (!utils::fexists(path)) {
            throw utils::exception("yaml=%s does not exist.", path);
        }
        auto doc = YAML::LoadFile(path.c_str());

        // base.
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

        // handler;
        if (auto node = doc["handler"]) {
            if (auto n = node["path"]) handler.path = n.as<std::string>();
            if (auto n = node["type"]) handler.parse_type(n.as<std::string>());
            if (auto n = node["indent"]) handler.indent = n.as<int>();
            if (auto n = node["sort_keys"]) handler.sort_keys = n.as<bool>();
            if (auto n = node["allow_non_ascii"]) handler.allow_non_ascii = n.as<bool>();
        }

        // fields
        fields.clear();
        for (auto node: doc["fields"]) {;
            auto field = Field();
            field.parse_type(node["type"].as<std::string>());
            field.column = node["column"].as<std::string>();
            field.name = node["name"].as<std::string>();
            field.parse_default(node["default"], path);
            field.validate = Validate();
            if (auto v = node["validate"]) {
                if (auto n = v["unique"]) field.validate.unique = n.as<bool>();
            }
            fields.push_back(std::move(field));
        }

        if (handler.sort_keys) {
            std::sort(fields.begin(), fields.end(), [](Field& a, Field& b) {
                return a.column < b.column;
            });
        }
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
                return utils::exception("");
            }
            case YAML::NodeType::Map: {
                return utils::exception("");
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
        try {
            return node.as<double>();
        } catch (const YAML::BadConversion& exc) {}
        if (intable) {
            return intval;
        }
        return strval;
    }
};

}
