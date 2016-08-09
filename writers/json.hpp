#pragma once
#include <type_traits>
#include <string>
#include <sstream>

namespace xlsxconverter {
namespace writers {

template<bool cond, class T = void>
using enable_if_t = typename std::enable_if<cond, T>::type;

struct JsonWriter
{
    std::stringstream buffer;

    bool is_first_row = false;
    bool is_first_field = false;

    void begin() {
        buffer.clear();
        buffer << "[";
        is_first_row = true;
    }

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

    void end_row() {
        if (is_first_field) {
            buffer << "}";
        } else if (is_first_row) {
            buffer << "    }";
        } else {
            buffer << "\n    }";
        }
    }

    void end() {
        buffer << "\n]\n";
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
    for (auto c: value) {
        switch (c) {
            case '"': { buffer << "\\\""; break; }
            case '\\': { buffer << "\\\\"; break; }
            default: { buffer << c; break; }
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
