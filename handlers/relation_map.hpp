#pragma once

#include <unordered_map>

#include "yaml_config.hpp"
#include "utils.hpp"

namespace xlsxconverter {
namespace handlers {

struct RelationMap
{
    static std::vector<RelationMap> cache_;

    YamlConfig& config;
    std::string column;
    std::string from;
    std::string key;
    YamlConfig::Field::Type column_type;
    YamlConfig::Field::Type key_type;
    // key -> colmun

    int column_index = -1;
    int key_index = -1;

    std::unordered_map<std::string, int64_t> s2imap;
    std::unordered_map<int64_t, int64_t> i2imap;

    bool current_column_handled = false;
    bool current_key_handled = false;

    int64_t     current_column_intvalue;
    std::string current_column_strvalue;
    int64_t     current_key_intvalue;
    std::string current_key_strvalue;

    inline
    RelationMap(YamlConfig::Field::Relation& relation, YamlConfig& config_)
    : column(relation.column), from(relation.from), key(relation.key), config(config_)
    {
        for (int i = 0; i < config.fields.size(); ++i) {
            auto& field = config.fields[i];
            if (field.column == column) column_index = i;
            if (field.column == key) key_index = i;
        }
        if (column_index == -1) throw utils::exception("relation column is not found.");
        if (key_index == -1) throw utils::exception("relation key is not found.");

        auto& column_field = config.fields[column_index];
        if (column_field.type != YamlConfig::Field::Type::kInt) {
            utils::exception("relation column type must be int.");
        }
        column_type = column_field.type;
        
        auto& key_field = config.fields[key_index];
        if (key_field.type != YamlConfig::Field::Type::kInt || key_field.type != YamlConfig::Field::Type::kChar) {
            utils::exception("relation key type must be int or char.");
        }
        key_type = key_field.type;
    }

    inline static
    bool has_cache(YamlConfig::Field::Relation& relation) {
        for (auto& rel: cache_) {
            if (rel == relation) return true;
        }
        return false;
    }

    inline static
    RelationMap& find_cache(YamlConfig::Field::Relation& relation) {
        for (auto& rel: cache_) {
            if (rel == relation) return rel;
        }
        throw utils::exception("relation_map is NOT exists.");
    }

    inline static
    void store_cache(RelationMap relmap) {
        cache_.push_back(std::move(relmap));
    }

    inline bool operator==(YamlConfig::Field::Relation& relation) {
        return column == relation.column && from == relation.from && key == relation.key;
    }

    template<class K, class V>
    V get(const K& k) { throw utils::exception("invalid type"); }

    inline
    void begin() {
    }

    inline
    void begin_row() {
        current_key_handled = false;
        current_column_handled = false;
    }

    template<class T>
    void field(YamlConfig::Field& field, const T& value) {
    }

    inline
    void end_row() {
        if (!current_key_handled) throw utils::exception("relation key cant handled.");
        if (!current_column_handled) throw utils::exception("relation column cant handled.");

        auto& f1 = config.fields[key_index];
        auto& f2 = config.fields[column_index];
        using FT = YamlConfig::Field::Type;
        if (f1.type == FT::kInt && f2.type == FT::kInt) {
            i2imap.insert(std::make_pair(current_key_intvalue, current_column_intvalue));
        } else if (f1.type == FT::kChar && f2.type == FT::kInt) {
            s2imap.insert(std::make_pair(current_key_strvalue, current_column_intvalue));
        }
    }

    inline
    void end() {}

    inline
    void save() {}
};

std::vector<RelationMap> RelationMap::cache_;


template<>
void RelationMap::field<std::string>(YamlConfig::Field& field, const std::string& value) {
    if (field.index == column_index) {
        current_column_strvalue = value;
        current_column_handled = true;
    } else if (field.index == key_index) {
        current_key_strvalue = value;
        current_key_handled = true;
    }
}

template<>
void RelationMap::field<int64_t>(YamlConfig::Field& field, const int64_t& value) {
    if (field.index == column_index) {
        current_column_intvalue = value;
        current_column_handled = true;
    } else if (field.index == key_index) {
        current_key_intvalue = value;
        current_key_handled = true;
    }
}

template<>
int64_t RelationMap::get<int64_t, int64_t>(const int64_t& value) {
    auto it = i2imap.find(value);
    if (it == i2imap.end()) throw utils::exception("relation: key=", value, ": not found.");
    return it->second;
}

template<>
int64_t RelationMap::get<std::string, int64_t>(const std::string& value) {
    auto it = s2imap.find(value);
    if (it == s2imap.end()) throw utils::exception("relation: key=", value, ": not found.");
    return it->second;
}



}
}