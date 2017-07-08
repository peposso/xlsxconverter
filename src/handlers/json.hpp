// Copyright (c) 2016 peposso All Rights Reserved.
// Released under the MIT license
#pragma once
#include <type_traits>
#include <string>
#include <sstream>
#include "yaml_config.hpp"
#include "utils.hpp"

#define DISABLE_ANY XLSXCONVERTER_UTILS_DISABLE_ANY
#define ENABLE_ANY  XLSXCONVERTER_UTILS_ENABLE_ANY

namespace xlsxconverter {
namespace handlers {

struct JsonHandler {
    YamlConfig::Handler& handler_config;
    YamlConfig& config;
    std::stringstream buffer;

    bool comment = false;
    bool is_first_row = false;
    bool is_first_field = false;

    std::string indent;
    std::string field_indent;
    std::string space;
    std::string endl;

    std::string name_quote;
    std::string name_separator;

    inline
    explicit JsonHandler(YamlConfig::Handler& handler_config_, YamlConfig& config_)
            : handler_config(handler_config_),
              config(config_),
              name_quote("\""),
              name_separator(":") {
        if (handler_config.indent < 0) {
            indent = space = endl = "";
        } else {
            for (int i = 0; i < handler_config.indent; ++i) {
                indent.push_back(' ');
            }
            if (handler_config.indent > 0) space = " ";
            endl = "\n";
        }
        field_indent = indent + indent;
    }

    inline
    void begin() {
        buffer.clear();
        buffer << "[";
        is_first_row = true;
    }

    inline
    void end() {
        buffer << endl << "]\n";
    }

    inline
    void begin_comment_row() {
        comment = true;
    }

    inline
    void end_comment_row() {
        comment = false;
    }

    inline
    void begin_row() {
        if (is_first_row) {
            buffer << endl;
            is_first_row = false;
        } else {
            buffer << ',' << endl;
        }
        buffer << indent << '{';
        is_first_field = true;
    }

    inline
    void end_row() {
        if (!is_first_field && !is_first_row) {
            buffer << endl << indent;
        }
        buffer << '}';
    }

    inline
    void write_key(const std::string& name) {
        buffer << name_quote << name << name_quote << name_separator << space;
    }

    template<class T, ENABLE_ANY(T, int64_t)>
    void write_value(const T& value) {
        buffer << value;
    }

    template<class T, ENABLE_ANY(T, double)>
    void write_value(const T& value) {
        auto s = std::to_string(value);
        if (s.find('.') == std::string::npos) {
            s.append(".0");
        }
        buffer << s;
    }

    template<class T, ENABLE_ANY(T, bool)>
    void write_value(const T& value) {
        buffer << (value ? "true" : "false");
    }

    template<class T, ENABLE_ANY(T, std::string)>
    void write_value(const T& value) {
        buffer << "\"";
        if (handler_config.allow_non_ascii) {
            for (auto c : value) {
                putchar_(c);
            }
        } else {
            for (auto uc : utils::u8to32iter(value)) {
                if (uc >= 0x80) {
                    bool sarrogate;
                    uint16_t c1, c2;
                    std::tie(sarrogate, c1, c2) = utils::u32to16char(uc);
                    if (sarrogate) {
                        buffer << "\\u" << std::setw(4) << std::setfill('0')
                               << std::hex << c1 << std::dec;
                        buffer << "\\u" << std::setw(4) << std::setfill('0')
                               << std::hex << c2 << std::dec;
                    } else {
                        buffer << "\\u" << std::setw(4) << std::setfill('0')
                               << std::hex << c1 << std::dec;
                    }
                } else {
                    putchar_(uc);
                }
            }
        }
        buffer << '"';
    }

    template<class T, ENABLE_ANY(T, std::nullptr_t)>
    void write_value(const T& value) {
        buffer << "null";
    }

    template<class T>
    void field(YamlConfig::Field& field, const T& value) {
        if (comment) return;
        if (is_first_field) {
            buffer << endl;
            is_first_field = false;
        } else {
            buffer << ',' << endl;
        }
        buffer << field_indent;
        write_key(field.column);
        write_value(value);
    }

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
            case '"': { buffer << "\\\""; break; }
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
