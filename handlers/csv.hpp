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

struct CSVHandler
{
    YamlConfig& config;
    std::stringstream buffer;

    bool is_first_row = false;
    bool is_first_field = false;
    bool is_header_row = false;

    const char endl = '\n';

    inline
    CSVHandler(YamlConfig& config) 
        : config(config)
    {}

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
        if (config.handler.csv_field_type) {
            begin_row();
            for (auto& f: config.fields) {
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
        if (config.handler.csv_field_column) {
            begin_row();
            for (auto& f: config.fields) {
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


    template<class T, ENABLE_ANY(T, int64_t, double)>
    void write_value(const T& value, ...) {
        buffer << value;
    }

    template<class T, ENABLE_ANY(T, bool)>
    void write_value(const T& value) {
        buffer << (value ? "true" : "false");
    }

    template<class T, ENABLE_ANY(T, std::string)>
    void write_value(const T& value) {
        for (auto c: value) {
            putchar_(c);
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
    void save() {
        utils::fs::writefile(config.get_output_path(), buffer.str());
        if (!config.arg_config.quiet) {
            utils::log("output: ", config.handler.path);
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


}
}
#undef DISABLE_ANY 
#undef ENABLE_ANY
