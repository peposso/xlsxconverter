// Copyright (c) 2016 peposso All Rights Reserved.
// Released under the MIT license
#pragma once
#include <type_traits>
#include <string>
#include <sstream>
#include "yaml_config.hpp"
#include "utils.hpp"
#include "relation_map.hpp"

#define DISABLE_ANY XLSXCONVERTER_UTILS_DISABLE_ANY
#define ENABLE_ANY  XLSXCONVERTER_UTILS_ENABLE_ANY

namespace xlsxconverter {
namespace handlers {

struct CSVHandler {
    YamlConfig& config;
    YamlConfig::Handler& handler_config;
    std::stringstream buffer;

    bool is_first_row = false;
    bool is_first_field = false;
    bool is_header_row = false;

    const char endl = '\n';

    inline
    explicit CSVHandler(YamlConfig::Handler& handler_config_, YamlConfig& config_)
        : handler_config(handler_config_),
          config(config_) { }

    inline
    void begin() {
        buffer.clear();
        is_first_row = true;
    }

    inline
    void begin_comment_row() {
        if (is_first_row) {
            is_first_row = false;
        } else {
            buffer << endl;
        }
        is_first_field = true;
    }

    inline
    void end_comment_row() {}

    inline
    void write_field_info_row() {
        if (handler_config.csv_field_type) {
            begin_row();
            for (auto& f : config.fields) {
                if (f.type == YamlConfig::Field::Type::kIsIgnored) continue;
                std::string type_name;
                if (f.type_alias.empty()) {
                    if (f.type == YamlConfig::Field::Type::kForeignKey) {
                        auto& relmap = RelationMap::find_cache(f.relation.value());
                        type_name = relmap.column_type_name;
                    } else {
                        type_name = f.type_name;
                    }
                } else {
                    type_name = f.type_alias;
                }
                field(f, type_name);
            }
        }
        if (handler_config.csv_field_column) {
            begin_row();
            for (auto& f : config.fields) {
                if (f.type == YamlConfig::Field::Type::kIsIgnored) continue;
                field(f, f.column);
            }
        }
    }

    inline
    void begin_row() {
        if (!is_header_row) {
            is_header_row = true;
            write_field_info_row();
            begin_row();
            return;
        }
        if (is_first_row) {
            is_first_row = false;
        } else {
            buffer << endl;
        }
        is_first_field = true;
    }

    inline
    void end_row() {}


    template<class T, ENABLE_ANY(T, int64_t)>
    void write_value(const T& value, ...) {
        buffer << value;
    }

    template<class T, ENABLE_ANY(T, double)>
    void write_value(const T& value, ...) {
        buffer << utils::dtos(value);
    }

    template<class T, ENABLE_ANY(T, bool)>
    void write_value(const T& value) {
        buffer << (value ? "true" : "false");
    }

    template<class T, ENABLE_ANY(T, std::string)>
    void write_value(const T& value) {
        if (value.find(',') != std::string::npos || value.find('\n') != std::string::npos) {
            buffer << '"';
            for (auto c : value) {
                if (c == '"') buffer << '"';
                buffer << c;
            }
            buffer << '"';
        } else {
            buffer << value;
        }
    }

    template<class T, ENABLE_ANY(T, std::nullptr_t)>
    void write_value(const T& value) {
        buffer << "null";
    }

    template<class T>
    void field(YamlConfig::Field& field, const T& value) {
        if (is_first_field) {
            is_first_field = false;
        } else {
            buffer << ',';
        }
        write_value(value);
    }

    inline
    void end() {}

    inline
    void save(ArgConfig& arg_config) {
        utils::fs::writefile(handler_config.get_output_path(), buffer.str());
        if (!arg_config.quiet) {
            utils::log("output: ", handler_config.path);
        }
    }

    inline
    void putchar_(char c) {
        switch (c) {
            case ',': { buffer << "\\,"; break; }
            case '\\': { buffer << "\\\\"; break; }
            case '\t': { buffer << "\\t"; break; }
            case '\r': { buffer << "\\r"; break; }
            case '\n': { buffer << "\\n"; break; }
            default: { buffer << c; break; }
        }
    }
};


}  // namespace handlers
}  // namespace xlsxconverter
#undef DISABLE_ANY
#undef ENABLE_ANY
