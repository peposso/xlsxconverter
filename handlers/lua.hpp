#pragma once

#include "json.hpp"

#define DISABLE_ANY XLSXCONVERTER_UTILS_DISABLE_ANY 
#define ENABLE_ANY  XLSXCONVERTER_UTILS_ENABLE_ANY

namespace xlsxconverter {
namespace handlers {

struct LuaHandler : public DjangoFixtureHandler
{
    inline
    LuaHandler(YamlConfig& config) 
        : DjangoFixtureHandler(config)
    {
        name_quote = "";
        name_separator = space + "=";
    }

    inline
    void begin() {
        buffer.clear();
        buffer << "return" + space + "{";
        is_first_row = true;
    }

    inline
    void end() {
        buffer << endl << "}\n";
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
        if (field.index == pk_index) {
            set_pk(value);
        }
    }

    template<class T, ENABLE_ANY(T, std::nullptr_t)>
    void write_value(const T& value) {
        buffer << "nil";
    }

    template<class T, DISABLE_ANY(T, std::nullptr_t)>
    void write_value(const T& value) {
        DjangoFixtureHandler::write_value(value);
    }
};

}
}
#undef DISABLE_ANY
#undef ENABLE_ANY
