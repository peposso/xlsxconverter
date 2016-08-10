#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>

#include <boost/any.hpp>
#include <yaml-cpp/yaml.h>

#include "util.hpp"

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
        Type type;
        std::string column;
        std::string name;
        bool using_default = false;
        boost::any default_value;

        void parse_type(std::string typestr) {
            if      (typestr == "int") { this->type = Type::kInt; }
            else if (typestr == "float") { this->type = Type::kFloat; }
            else if (typestr == "bool") { this->type = Type::kBool; }
            else if (typestr == "char") { this->type = Type::kChar; }
            else if (typestr == "datetime") { this->type = Type::kDateTime; }
            else if (typestr == "foreignkey") { this->type = Type::kForeignKey; }
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
        if (!util::fexists(path)) {
            throw util::exception("yaml=%s does not exist.", path);
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
            if (auto o = node["path"]) handler.path = o.as<std::string>();
            if (auto o = node["type"]) handler.parse_type(o.as<std::string>());
            if (auto o = node["indent"]) handler.indent = o.as<int>();
            if (auto o = node["sort_keys"]) handler.sort_keys = o.as<bool>();
            if (auto o = node["allow_non_ascii"]) handler.allow_non_ascii = o.as<bool>();
        }

        // fields
        fields.clear();
        for (auto node: doc["fields"]) {;
            auto field = Field();
            field.parse_type(node["type"].as<std::string>());
            field.column = node["column"].as<std::string>();
            field.name = node["name"].as<std::string>();
            fields.push_back(std::move(field));
        }

        if (handler.sort_keys) {
            std::sort(fields.begin(), fields.end(), [](Field& a, Field& b) {
                return a.column < b.column;
            });
        }
    }

    std::string get_xls_path() {
        return arg_config.xls_search_path + '/' + target_xls_path;
    }

    std::string get_output_path() {
        return arg_config.output_base_path + '/' + handler.path;
    }
};

}
