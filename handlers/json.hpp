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

    inline
    JsonHandler(YamlConfig& config) : config(config) {}

    inline
    void begin() {
        buffer.clear();
        buffer << "[";
        is_first_row = true;
    }

    inline
    void begin_row() {
        if (is_first_row) {
            buffer << "\n";
            is_first_row = false;
        } else {
            buffer << ",\n";
        }
        buffer << "    {";
        is_first_field = true;
    }

    inline
    void write_key(const std::string& name) {
        if (is_first_field) {
            buffer << "\n";
            is_first_field = false;
        } else {
            buffer << ",\n";
        }
        buffer << "        \"" << name << "\": ";
    }

    template<class T>
    void field(YamlConfig::Field& field, const T& value) {
        write_key(field.column);
        buffer << value;
    }

    inline
    void end_row() {
        if (is_first_field) {
            buffer << "}";
        } else if (is_first_row) {
            buffer << "    }";
        } else {
            buffer << "\n    }";
        }
    }

    inline
    void end() {
        buffer << "\n]\n";
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
void JsonHandler::field<bool>(YamlConfig::Field& field, const bool& value) {
    write_key(field.column);
    buffer << (value ? "true" : "false");
}


template<>
void JsonHandler::field<std::string>(YamlConfig::Field& field, const std::string& value) {
    write_key(field.column);
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
    buffer << "\"";
}

template<>
void JsonHandler::field<std::nullptr_t>(YamlConfig::Field& field, const std::nullptr_t& value) {
    write_key(field.column);
    buffer << "null";
}


}
}
