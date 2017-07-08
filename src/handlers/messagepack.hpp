// Copyright (c) 2016 peposso All Rights Reserved.
// Released under the MIT license
#pragma once
#include <type_traits>
#include <string>
#include <sstream>
#include <msgpack.hpp>
#include "yaml_config.hpp"
#include "utils.hpp"
#include "relation_map.hpp"

#define DISABLE_ANY XLSXCONVERTER_UTILS_DISABLE_ANY
#define ENABLE_ANY  XLSXCONVERTER_UTILS_ENABLE_ANY

namespace xlsxconverter {
namespace handlers {

namespace strutil = utils::strutil;

struct MessagePackHandler {

    YamlConfig& config;
    YamlConfig::Handler& handler_config;
    std::stringstream buffer;

    bool is_first_row = false;
    bool is_first_field = false;
    bool is_header_row = false;

    struct strbuf {
        std::string s;
        int i;
        int j;

        strbuf(const std::string& s, int i, int j)
            : s(s), i(i), j(j) { }
    };

    std::vector<strbuf> strbufstack;
    std::vector<std::vector<msgpack::object>> table;

    inline
    explicit MessagePackHandler(YamlConfig::Handler& handler_config_, YamlConfig& config_)
        : handler_config(handler_config_),
          config(config_) { }

    inline
    void begin_comment_row() {
    }

    inline
    void end_comment_row() {
    }

    inline
    void begin() {
        buffer.clear();
        is_first_row = true;
    }

    inline
    void write_field_info_row() {
        if (!handler_config.messagepack_no_header) {
            for (auto& f : config.fields) {
                if (f.type == YamlConfig::Field::Type::kIsIgnored) continue;
                if (handler_config.messagepack_upper_camelize) {
                    field(f, strutil::upper_camel(f.column));
                } else {
                    field(f, f.column);
                }
            }
        }
    }

    inline
    void begin_row() {
        if (is_first_row) {
            is_first_row = false;
            table.emplace_back();
            write_field_info_row();
            is_first_field = true;
            table.emplace_back();
            return;
        }
        table.emplace_back();
        is_first_field = true;
    }

    inline
    void end_row() {}

    template<class T, ENABLE_ANY(T, int64_t)>
    void write_value(const T& value, ...) {
        table.back().emplace_back(value);
    }

    template<class T, ENABLE_ANY(T, double)>
    void write_value(const T& value, ...) {
        table.back().emplace_back(value);
    }

    template<class T, ENABLE_ANY(T, bool)>
    void write_value(const T& value) {
        table.back().emplace_back(value);
    }

    template<class T, ENABLE_ANY(T, std::string)>
    void write_value(const T& value) {
        // assign later for escaping pointer address move.
        strbufstack.emplace_back(value, table.size()-1, table.back().size());
        table.back().emplace_back();
    }

    template<class T, ENABLE_ANY(T, std::nullptr_t)>
    void write_value(const T& value) {
        table.back().emplace_back(msgpack::type::nil_t());
    }

    template<class T>
    void field(YamlConfig::Field& field, const T& value) {
        if (is_first_field) {
            is_first_field = false;
        }
        write_value(value);
    }

    inline
    void end() {}

    inline
    void save(ArgConfig& arg_config) {
        for (auto& strbuf : strbufstack) {
            table[strbuf.i][strbuf.j] = msgpack::object(strbuf.s.c_str());
        }
        msgpack::pack(buffer, table);
        auto s = buffer.str();
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
