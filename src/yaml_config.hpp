// Copyright (c) 2016 peposso All Rights Reserved.
// Released under the MIT license
#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include <boost/optional.hpp>  // NOLINT
#include <boost/any.hpp>  // NOLINT
#include <yaml-cpp/yaml.h>  // NOLINT

#include "utils.hpp"

#define EXCEPTION XLSXCONVERTER_UTILS_EXCEPTION

namespace xlsxconverter {

struct YamlConfig {
    struct Handler {
        enum Type {
            kError, kNone, kJson, kCSV, kDjangoFixture, kLua, kTemplate, kEnum,
        };
        Type type = kError;
        std::string type_name;
        std::string path;
        std::string source;
        int indent = 4;
        // for json
        bool sort_keys = false;
        bool allow_non_ascii = false;
        // for csv
        boost::optional<int> comment_row = boost::none;
        bool csv_field_type = false;
        bool csv_field_column = false;
        YAML::Node context;

        static inline
        Type parse_type(const std::string& name) {
            static const std::unordered_map<std::string, Type> map = {
                {"none", Type::kNone},
                {"json", Type::kJson},
                {"csv", Type::kCSV},
                {"djangofixture", Type::kDjangoFixture},
                {"lua", Type::kLua},
                {"template", Type::kTemplate},
            };
            return map.at(name);
        }

        inline Handler() {}
        inline explicit Handler(YAML::Node node) {
            auto typepath = utils::split(node["type"].as<std::string>(), '.');
            type_name = typepath[typepath.size()-1];
            try {
                type = parse_type(type_name);
            } catch (std::out_of_range&) {
                throw EXCEPTION("unknown handler.type: ", type_name);
            }

            if (auto n = node["path"]) path = n.as<std::string>();
            if (auto n = node["source"]) source = n.as<std::string>();
            if (auto n = node["indent"]) indent = n.as<int>();
            if (auto n = node["sort_keys"]) sort_keys = n.as<bool>();
            if (auto n = node["allow_non_ascii"]) allow_non_ascii = n.as<bool>();

            if (auto n = node["comment_row"]) comment_row = n.as<int>();
            if (auto n = node["csv_field_type"]) csv_field_type = n.as<bool>();
            if (auto n = node["csv_field_column"]) csv_field_column = n.as<bool>();
            context = node["context"];
        }
    };
    struct Field {
        enum Type {
            kError, kInt, kFloat, kBool, kChar, kDateTime, kUnixTime,
            kAny, kForeignKey, kIsIgnored,
        };
        struct Validate {
            bool unique = false;
            bool sorted = false;
            bool sequential = false;
            boost::optional<int64_t> max = boost::none;
            boost::optional<int64_t> min = boost::none;
            bool anyof = false;
            std::unordered_set<int64_t> anyof_intset;
            std::unordered_set<std::string> anyof_strset;

            inline explicit Validate(YAML::Node node) {
                if (auto n = node["unique"]) unique = n.as<bool>();
                if (auto n = node["sorted"]) sorted = n.as<bool>();
                if (auto n = node["sequential"]) sequential = n.as<bool>();
                if (auto n = node["max"]) max = n.as<int64_t>();
                if (auto n = node["min"]) min = n.as<int64_t>();
                if (auto n = node["anyof"]) {
                    anyof = true;
                    for (auto item : n) {
                        auto s = item.as<std::string>();
                        anyof_strset.insert(s);
                        if (utils::isdecimal(s)) {
                            anyof_intset.insert(std::stoll(s));
                        }
                    }
                }
            }
        };
        struct Relation {
            std::string column;
            std::string from;
            std::string key;
            std::string id;
            inline explicit Relation(YAML::Node node) {
                if (auto n = node["column"]) column = n.as<std::string>();
                if (auto n = node["from"]) from = n.as<std::string>();
                if (auto n = node["key"]) key = n.as<std::string>();
                id = column + ':' + from + ':' + key;
            }
        };
        std::string type_name;
        std::string type_alias;
        Type type;
        std::string column;
        std::string name;
        bool using_default = false;
        bool optional = false;
        boost::any default_value;
        boost::optional<Validate> validate = boost::none;
        boost::optional<Relation> relation = boost::none;
        boost::optional<std::unordered_map<std::string, std::string>> definition = boost::none;
        int index = -1;

        static inline
        Type parse_type(const std::string& name) {
            static const std::unordered_map<std::string, Type> map = {
                {"int", Type::kInt},
                {"float", Type::kFloat},
                {"bool", Type::kBool},
                {"char", Type::kChar},
                {"datetime", Type::kDateTime},
                {"unixtime", Type::kUnixTime},
                {"any", Type::kAny},
                {"foreignkey", Type::kForeignKey},
                {"isignored", Type::kIsIgnored},
            };
            return map.at(name);
        }

        inline explicit Field(YAML::Node node) {
            column = node["column"].as<std::string>();
            name = node["name"].as<std::string>();

            type_name = node["type"].as<std::string>();
            try {
                type = parse_type(type_name);
            } catch (std::out_of_range&) {
                throw EXCEPTION("unknown field.type: ", type_name);
            }

            if (auto o = node["type_alias"]) type_alias = o.as<std::string>();
            if (auto o = node["optional"]) optional = o.as<bool>();

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

            if (auto n = node["definition"]) {
                if (using_default) {
                    throw EXCEPTION("using 'default' and 'definition' at same field.");
                }
                std::unordered_map<std::string, std::string> map;
                for (auto kv : n) {
                    map.emplace(kv.first.as<std::string>(), kv.second.as<std::string>());
                }
                definition = map;
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
        : path(path_),
          arg_config(arg_config_) {
        std::string fullpath = arg_config.search_yaml_path(path);
        YAML::Node doc;
        try {
            doc = YAML::LoadFile(fullpath.c_str());
        } catch (std::exception& exc) {
            throw EXCEPTION(path, ": ", exc.what());
        }

        // root.
        if (doc["name"]) {
            name = doc["name"].as<std::string>();
        } else {
            auto p = path.rfind('.');
            if (p != std::string::npos) {
                name = path.substr(0, p);
            } else {
                name = path;
            }
            for (auto& c : name) if (c == '/') c = '_';
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
            try {
                handler = Handler(node);
            } catch (std::exception& exc) {
                throw EXCEPTION(path, ": ", exc.what());
            }
        }

        // fields
        fields.clear();
        for (auto node : doc["fields"]) {
            try {
                fields.push_back(Field(node));
            } catch (std::exception& exc) {
                throw EXCEPTION(path, ": ", exc.what());
            }
        }

        if (handler.sort_keys) {
            std::sort(fields.begin(), fields.end(), [](Field& a, Field& b) {
                return a.column < b.column;
            });
        }

        int index = 0;
        for (auto& field : fields) {
            field.index = index++;
        }
    }

    inline
    std::vector<Field::Relation> relations() {
        auto vec = std::vector<Field::Relation>();
        for (auto& field : fields) {
            if (field.relation != boost::none) {
                vec.push_back(field.relation.value());
            }
        }
        return vec;
    }

    inline
    std::vector<std::string> get_xls_paths() {
        std::vector<std::string> paths;
        if (target_xls_path.find('*') == std::string::npos) {
            paths.push_back(utils::fs::joinpath(arg_config.xls_search_path, target_xls_path));
            return paths;
        }
        auto dir = utils::fs::dirname(target_xls_path);
        auto pattern = utils::fs::basename(target_xls_path);
        if (dir.find('*') != std::string::npos) {
            throw EXCEPTION(path, ": not supported target pattern=", target_xls_path);
        }
        auto fulldir = utils::fs::joinpath(arg_config.xls_search_path, dir);
        for (auto& entry : utils::fs::iterdir(fulldir)) {
            if (!entry.isfile) continue;
            if (entry.name[0] == '~') continue;
            if (utils::fs::match(entry.name, pattern)) {
                paths.push_back(utils::fs::joinpath(arg_config.xls_search_path, dir, entry.name));
            }
        }
        std::sort(paths.begin(), paths.end());
        return paths;
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
                throw EXCEPTION("bad default type: sequence.");
            }
            case YAML::NodeType::Map: {
                throw EXCEPTION("bad default type: map.");
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

}  // namespace xlsxconverter

#undef EXCEPTION
