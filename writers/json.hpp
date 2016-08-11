#pragma once
#include <type_traits>
#include <string>
#include <sstream>
#include "yaml_config.hpp"
#include "utils.hpp"

namespace xlsxconverter {
namespace writers {

struct JsonWriter
{
    YamlConfig& config;
    std::stringstream buffer;

    bool is_first_row = false;
    bool is_first_field = false;

    inline
    JsonWriter(YamlConfig& config) : config(config) {}

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
    void write_name(const std::string& name) {
        if (is_first_field) {
            buffer << "\n";
            is_first_field = false;
        } else {
            buffer << ",\n";
        }
        buffer << "        \"" << name << "\": ";
    }

    template<class T>
    void field(const std::string& name, const T& value) {
        write_name(name);
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
void JsonWriter::field<bool>(const std::string& name, const bool& value) {
    write_name(name);
    buffer << (value ? "true" : "false");
}


template<>
void JsonWriter::field<std::string>(const std::string& name, const std::string& value) {
    write_name(name);
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
void JsonWriter::field<std::nullptr_t>(const std::string& name, const std::nullptr_t& value) {
    write_name(name);
    buffer << "null";
}


}
}
