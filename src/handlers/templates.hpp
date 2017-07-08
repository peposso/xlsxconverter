// Copyright (c) 2016 peposso All Rights Reserved.
// Released under the MIT license
#pragma once
#include <type_traits>
#include <string>
#include <sstream>

#include <mustache/mustache.hpp>

#include "yaml_config.hpp"
#include "utils.hpp"
#include "relation_map.hpp"

#define DISABLE_ANY XLSXCONVERTER_UTILS_DISABLE_ANY
#define ENABLE_ANY  XLSXCONVERTER_UTILS_ENABLE_ANY

namespace xlsxconverter {
namespace handlers {

namespace strutil = utils::strutil;

struct TemplateHandler {
    using Mustache = Kainjow::Mustache;
    using Data = Kainjow::Mustache::Data;

    YamlConfig& config;
    YamlConfig::Handler& handler_config;
    std::stringstream buffer;

    Mustache template_;
    Data records;
    Data current_record;
    Data current_record_fields;

    inline
    explicit TemplateHandler(YamlConfig::Handler& handler_config_, YamlConfig& config_)
            : handler_config(handler_config_),
              config(config_),
              template_((utils::fs::readfile(handler_config.source))),
              records(Data::Type::List),
              current_record_fields(Data::Type::List),
              current_record() {
        #define LAMBDA() [](const Data* data, const std::string& arg, Mustache::Context* ctx) -> const Data*
        template_.registerFilter("upper", LAMBDA() {
            if (!data->isString()) return data;
            return ctx->addPool(Data(strutil::upper(data->stringValue())));
        });
        template_.registerFilter("lower", LAMBDA() {
            if (!data->isString()) return data;
            return ctx->addPool(Data(strutil::lower(data->stringValue())));
        });
        template_.registerFilter("snake_case", LAMBDA() {
            if (!data->isString()) return data;
            return ctx->addPool(Data(strutil::snake_case(data->stringValue())));
        });
        template_.registerFilter("upper_camel", LAMBDA() {
            if (!data->isString()) return data;
            return ctx->addPool(Data(strutil::upper_camel(data->stringValue())));
        });
        template_.registerFilter("lower_camel", LAMBDA() {
            if (!data->isString()) return data;
            return ctx->addPool(Data(strutil::lower_camel(data->stringValue())));
        });
        template_.registerFilter("eq:", LAMBDA() {
            if (!data->isString()) return data;
            return ctx->addPool(Data(data->stringValue() == arg ? Data::Type::True : Data::Type::False));
        });
        template_.registerFilter("ne:", LAMBDA() {
            if (!data->isString()) return data;
            return ctx->addPool(Data(data->stringValue() != arg ? Data::Type::True : Data::Type::False));
        });
        #undef LAMBDA
    }

    inline static utils::mutex_map<std::string, Mustache>& template_cache() {
        static utils::mutex_map<std::string, Mustache> cache_;
        return cache_;
    }

    inline
    void begin() {
        buffer.clear();
    }

    inline
    void begin_comment_row() {}

    inline
    void end_comment_row() {}

    inline
    void begin_row() {
        current_record = Data();
        current_record_fields = Data(Data::Type::List);
    }

    template<class T, DISABLE_ANY(T, bool, std::nullptr_t)>
    void field(YamlConfig::Field& field, const T& value) {
        std::stringstream ss;
        ss << value;
        append(field, ss.str());
    }

    template<class T, ENABLE_ANY(T, bool)>
    void field(YamlConfig::Field& field, const T& value) {
        append(field, value ? "true" : "false");
    }

    template<class T, ENABLE_ANY(T, std::nullptr_t)>
    void field(YamlConfig::Field& field, const T& value) {
        append(field, "null");
    }

    void append(YamlConfig::Field& field, std::string s) {
        Data field_data;
        field_data.set("column", Data(field.column));
        field_data.set("name", Data(field.name));
        field_data.set("type", Data(field.type_name));
        field_data.set("value", Data(s));
        current_record_fields.push_back(field_data);
        current_record.set(field.column, Data(s));
    }

    inline
    void end_row() {
        current_record.set("fields", current_record_fields);
        records.push_back(current_record);
    }

    inline Data yaml2data(YAML::Node node) {
        if (node.Type() == YAML::NodeType::Scalar) {
            return Data(node.as<std::string>());
        }
        if (node.Type() != YAML::NodeType::Map) {
            return Data();
        }
        Data map;
        for (auto it : node) {
            map.set(it.first.as<std::string>(), yaml2data(it.second));
        }
        return map;
    }

    inline
    void end() {
        Data context;
        if (handler_config.context.Type() == YAML::NodeType::Map) {
            context = yaml2data(handler_config.context);
        }
        context.set("records", records);
        buffer << template_.render(context);
    }

    inline
    void save(ArgConfig& arg_config) {
        utils::fs::writefile(handler_config.get_output_path(), buffer.str());
        if (!arg_config.quiet) {
            utils::log("output: ", handler_config.path);
        }
    }
};


}  // namespace handlers
}  // namespace xlsxconverter
#undef DISABLE_ANY
#undef ENABLE_ANY
