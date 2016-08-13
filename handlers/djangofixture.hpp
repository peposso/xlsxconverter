#pragma once

#include "json.hpp"

namespace xlsxconverter {
namespace handlers {

struct DjangoFixtureHandler : public JsonHandler
{
    int pk_index = -1;
    int64_t pk_intvalue = -1;
    std::string pk_strvalue;

    inline
    DjangoFixtureHandler(YamlConfig& config) 
        : JsonHandler(config)
    {
        for (auto& field: config.fields) {
            if (field.column == "ID" || field.column == "id" || field.column == "Id") {
                pk_index = field.index;
            }
        }
        if (pk_index == -1) {
            throw utils::exception("id field is not found.");
        }
    }

    inline
    void begin_row() {
        JsonHandler::begin_row();
        buffer << endl << indent << indent << "\"fields\":" << space << '{';
        pk_strvalue.clear();
        pk_intvalue = -1;
    }

    inline
    void write_key(const std::string& name) {
        if (is_first_field) {
            buffer << endl;
            is_first_field = false;
        } else {
            buffer << ',' << endl;
        }
        buffer << indent << indent << indent << '"' << name << "\":" << space;
    }

    template<class T>
    void set_pk(T t, ...) {
        throw utils::exception("bad pk type");
    }
    void set_pk(int64_t v, boost::type<int64_t>) {
        pk_intvalue = v;
    }
    void set_pk(const std::string& v, boost::type<std::string>) {
        pk_strvalue = v;
    }

    template<class T>
    void field(YamlConfig::Field& field, const T& value) {
        write_key(field.column);
        write_value(value);
        if (field.index == pk_index) {
            set_pk(value, boost::type<T>());
        }
    }

    inline
    void end_row() {
        if (!is_first_field && !is_first_row) {
            buffer << endl << indent << indent;
        }
        buffer << "}," << endl;
        buffer << indent << indent << "\"pk\":" << space;
        if (pk_intvalue > -1) {
            write_value(pk_intvalue);
        } else if (!pk_strvalue.empty()) {
            write_value(pk_strvalue);
        } else {
            throw utils::exception("pk column not found.");
        }
        buffer << endl << indent << "}";
    }
};

}
}