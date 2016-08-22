#pragma once

#include <unordered_map>
#include <mutex>

#include "yaml_config.hpp"
#include "utils.hpp"

#define DISABLE_ANY XLSXCONVERTER_UTILS_DISABLE_ANY 
#define ENABLE_ANY  XLSXCONVERTER_UTILS_ENABLE_ANY
#define EXCEPTION XLSXCONVERTER_UTILS_EXCEPTION

namespace xlsxconverter {
namespace handlers {

struct RelationMap
{
    inline static utils::mutex_map<std::string, RelationMap>& cache() {
        static utils::mutex_map<std::string, RelationMap> cache_;
        return cache_;
    }

    YamlConfig& config;
    std::string id;
    std::string column;
    std::string from;
    std::string key;
    YamlConfig::Field::Type column_type;
    YamlConfig::Field::Type key_type;
    std::string column_type_name;
    std::string key_type_name;
    // key -> colmun

    int column_index = -1;
    int key_index = -1;

    std::unordered_map<std::string, int64_t> s2imap;
    std::unordered_map<int64_t, int64_t> i2imap;

    bool current_column_handled = false;
    bool current_key_handled = false;
    bool comment = false;

    int64_t     current_column_intvalue;
    std::string current_column_strvalue;
    int64_t     current_key_intvalue;
    std::string current_key_strvalue;

    inline
    RelationMap(YamlConfig::Field::Relation& relation, YamlConfig& config_)
        : column(relation.column),
          from(relation.from),
          key(relation.key),
          id(relation.id),
          column_type_name(), key_type_name(),
          config(config_)
    {
        for (int i = 0; i < config.fields.size(); ++i) {
            auto& field = config.fields[i];
            if (field.column == column) column_index = i;
            if (field.column == key) key_index = i;
        }
        if (column_index == -1) throw EXCEPTION("relation column is not found.");
        if (key_index == -1) throw EXCEPTION("relation key is not found.");

        auto& column_field = config.fields[column_index];
        if (column_field.type != YamlConfig::Field::Type::kInt) {
            throw EXCEPTION("relation column type must be int.");
        }
        column_type = column_field.type;
        column_type_name = column_field.type_name;
        
        auto& key_field = config.fields[key_index];
        if (key_field.type != YamlConfig::Field::Type::kInt && key_field.type != YamlConfig::Field::Type::kChar) {
            throw EXCEPTION("relation key type must be int or char.");
        }
        key_type = key_field.type;
        key_type_name = key_field.type_name;
    }

    inline static
    bool has_cache(YamlConfig::Field::Relation& relation) {
        return cache().has(relation.id);;
    }

    inline static
    RelationMap& find_cache(YamlConfig::Field::Relation& relation) {
        auto relmap = cache().getref(relation.id);
        if (relmap == boost::none) {
            {
                std::lock_guard<decltype(cache().mutex)> lock(cache().mutex);
                for (const auto& kv: cache().map) {
                    utils::logerr("cache-key=", kv.first);
                }
            }
            throw EXCEPTION("relation_map ", relation.id, " is NOT exists.");
        }
        assert(relmap->id == relation.id);
        return relmap.value();
    }

    inline static
    void store_cache(RelationMap&& relmap) {
        auto id = relmap.id;
        cache().emplace(relmap.id, std::move(relmap));
    }

    inline bool operator==(YamlConfig::Field::Relation& relation) {
        return column == relation.column && from == relation.from && key == relation.key;
    }

    template<class V, class K, DISABLE_ANY(V, int64_t)>
    int get(const K& k) { throw EXCEPTION("invalid type"); }

    template<class V, class K, ENABLE_ANY(V, int64_t), DISABLE_ANY(K, int64_t, std::string)>
    int get(const K& k) { throw EXCEPTION("invalid type"); }

    template<class V, class K, ENABLE_ANY(V, int64_t), ENABLE_ANY(K, int64_t)>
    V get(const K& key) {
        auto it = i2imap.find(key);
        if (it == i2imap.end()) throw EXCEPTION("relation: key=", key, ": not found.");
        return it->second;
    }

    template<class V, class K, ENABLE_ANY(V, int64_t), ENABLE_ANY(K, std::string)>
    V get(const K& key) {
        auto it = s2imap.find(key);
        if (it == s2imap.end()) throw EXCEPTION("relation: key=", key, ": not found.");
        return it->second;
    }

    inline
    void begin() {}

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
        current_key_handled = false;
        current_column_handled = false;
    }

    inline
    void end_row() {
        // if (!current_key_handled) return;
        // if (!current_column_handled) return;
        if (!current_key_handled) {
            auto& field = config.fields[key_index];
            throw EXCEPTION("relation key=", field.column, " cant handled.");
        }
        if (!current_column_handled) {
            auto& field = config.fields[column_index];
            throw EXCEPTION("relation column=", field.column, " cant handled.");
        }

        auto& f1 = config.fields[key_index];
        auto& f2 = config.fields[column_index];
        using FT = YamlConfig::Field::Type;
        if (f1.type == FT::kInt && f2.type == FT::kInt) {
            i2imap.insert(std::make_pair(current_key_intvalue, current_column_intvalue));
        } else if (f1.type == FT::kChar && f2.type == FT::kInt) {
            s2imap.insert(std::make_pair(current_key_strvalue, current_column_intvalue));
        }
    }

    template<class T, DISABLE_ANY(T, int64_t, std::string)>
    void field(YamlConfig::Field& field, const T& value) {
        if (comment) return;
        if (field.index == column_index || field.index == key_index) {
            throw EXCEPTION("pk field.type requires int or str.");
        }
    }

    template<class T, ENABLE_ANY(T, std::string)>
    void field(YamlConfig::Field& field, const T& value) {
        if (comment) return;
        if (field.index == column_index) {
            current_column_strvalue = value;
            current_column_handled = true;
        }
        if (field.index == key_index) {
            current_key_strvalue = value;
            current_key_handled = true;
        }
    }

    template<class T, ENABLE_ANY(T, int64_t)>
    void field(YamlConfig::Field& field, const T& value) {
        if (comment) return;
        if (field.index == column_index) {
            current_column_intvalue = value;
            current_column_handled = true;
        }
        if (field.index == key_index) {
            current_key_intvalue = value;
            current_key_handled = true;
        }
    }

    inline
    void end() {}

    inline
    void save() {}
};

}
}
#undef EXCEPTION
#undef DISABLE_ANY
#undef ENABLE_ANY
