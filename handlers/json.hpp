#pragma once
#include <type_traits>
#include <string>
#include <sstream>
#include "yaml_config.hpp"
#include "utils.hpp"

namespace xlsxconverter {
namespace handlers {

struct JsonHandler
{
    YamlConfig& config;
    std::stringstream buffer;

    bool is_first_row = false;
    bool is_first_field = false;

    std::string indent;
    std::string space;
    std::string endl;

    inline
    JsonHandler(YamlConfig& config) 
        : config(config)
    {
        if (config.handler.indent < 0) {
            indent = space = endl = "";
        } else {
            for (int i = 0; i < config.handler.indent; ++i) {
                indent.push_back(' ');
            }
            if (config.handler.indent > 0) space = " ";
            endl = "\n";
        }
    }

    inline
    void begin() {
        buffer.clear();
        buffer << "[";
        is_first_row = true;
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
    void write_key(const std::string& name) {
        if (is_first_field) {
            buffer << endl;
            is_first_field = false;
        } else {
            buffer << ',' << endl;
        }
        buffer << indent << indent << '"' << name << "\":" << space;
    }

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
        buffer << "\"";
        if (config.handler.allow_non_ascii) {
            for (auto c: value) {
                putchar_(c);
            }
        } else {
            for (auto uc: utils::u8to32iter(value)) {
                if (uc >= 0x80) {
                    buffer << "\\u" << std::hex << uc << std::dec;
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
        write_key(field.column);
        write_value(value);
    }

    inline
    void end_row() {
        if (!is_first_field && !is_first_row) {
            buffer << endl << indent;
        }
        buffer << '}';
    }

    inline
    void end() {
        buffer << endl << "]\n";
    }

    inline
    void save() {
        auto fo = std::ofstream(config.get_output_path().c_str(), std::ios::binary);
        fo << buffer.str();
        if (!config.arg_config.quiet) {
            utils::log(config.handler.path, " writed.");
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

template<>
void JsonHandler::write_value<bool>(const bool& value) {
    buffer << (value ? "true" : "false");
}


template<>
void JsonHandler::write_value<std::string>(const std::string& value) {
    buffer << "\"";
    if (config.handler.allow_non_ascii) {
        for (auto c: value) {
            putchar_(c);
        }
    } else {
        for (auto uc: utils::u8to32iter(value)) {
            if (uc >= 0x80) {
                buffer << "\\u" << std::hex << uc << std::dec;
            } else {
                putchar_(uc);
            }
        }
    }
    buffer << '"';
}

template<>
void JsonHandler::write_value<std::nullptr_t>(const std::nullptr_t& value) {
    buffer << "null";
}


}
}
