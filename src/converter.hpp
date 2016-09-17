// Copyright (c) 2016 peposso All Rights Reserved.
// Released under the MIT license
#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <memory>

#include "xlsx.hpp"
#include "utils.hpp"

#include "yaml_config.hpp"
#include "arg_config.hpp"

#include "handlers.hpp"
#include "validator.hpp"

#define EXCEPTION XLSXCONVERTER_UTILS_EXCEPTION

namespace xlsxconverter {

struct Converter {
    static inline
    bool truthy(const std::string& s) {
        static std::unordered_set<std::string> set = {
            "false", "False", "FALSE",
            "no", "No", "NO",
            "non", "Non", "NON",
            "n", "N",
            "off", "Off", "OFF",
            "null", "Null", "NULL",
            "nil", "Nill", "NILL",
            "none", "None", "NONE",
            "0", "0.0", "",
        };
        return set.find(s) == set.end();
    }


    YamlConfig& config;
    bool ignore_relation = false;
    bool using_cache = false;
    std::vector<boost::optional<Validator>> validators;
    std::vector<boost::optional<handlers::RelationMap&>> relations;

    inline
    explicit Converter(YamlConfig& config_, bool using_cache_, bool ignore_relation_ = false)
            : config(config_),
              using_cache(using_cache_),
              ignore_relation(ignore_relation_) {
        for (auto& field : config.fields) {
            if (field.validate == boost::none) {
                validators.push_back(boost::none);
            } else {
                validators.push_back(Validator(field));
            }
            if (ignore_relation || field.relation == boost::none) {
                relations.push_back(boost::none);
            } else {
                relations.push_back(handlers::RelationMap::find_cache(field.relation.value()));
            }
        }
        if (config.arg_config.no_cache) {
            using_cache = false;
        }
    }

    static inline
    std::shared_ptr<xlsx::Workbook> open_workbook(const std::string& path, bool using_cache) {
        if (!using_cache) {
            return std::make_shared<xlsx::Workbook>(path);
        }
        static std::unordered_map<std::string, std::shared_ptr<xlsx::Workbook>> cache;
        static std::mutex m;
        std::lock_guard<std::mutex> lock(m);
        auto it = cache.find(path);
        if (it != cache.end()) {
            return it->second;
        }
        auto ptr = std::make_shared<xlsx::Workbook>(path);
        cache.emplace(path, ptr);
        return ptr;
    }

    template<class T>
    void run(T& handler) {
        auto paths = config.get_xls_paths();
        if (paths.empty()) {
            throw EXCEPTION(config.path, ": target file does not exist.");
        }
        handler.begin();
        for (int i = 0; i < paths.size(); ++i) {
            auto xls_path = paths[i];
            try {
                auto book = open_workbook(xls_path, using_cache);
                auto& sheet = book->sheet_by_name(config.target_sheet_name);
                auto column_mapping = map_column(sheet, xls_path);
                // process data
                handle(handler, sheet, column_mapping);
            } catch (utils::exception& exc) {
                throw EXCEPTION(config.path, ": ", xls_path, ": ", exc.what());
            }
        }
        handler.end();
    }

    inline
    std::vector<int> map_column(xlsx::Sheet& sheet, std::string& xls_path) {
        std::vector<int> column_mapping;
        for (int k = 0; k < config.fields.size(); ++k) {
            auto& field = config.fields[k];
            bool found = false;
            for (int i = 0; i < sheet.ncols(); ++i) {;
                auto& cell = sheet.cell(config.row - 1, i);
                if (cell.as_str() == field.name) {
                    column_mapping.push_back(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (field.optional) {
                    column_mapping.push_back(-1);
                    continue;
                }
                for (int i = 0; i < sheet.ncols(); ++i) {
                    auto& cell = sheet.cell(config.row-1, i);
                    utils::log("cell[", cell.cellname(), "]=", cell.as_str());
                }
                throw EXCEPTION(config.path, ": ", xls_path, ": row=", config.row,
                                ": field{column=", field.column,
                                ",name=", field.name, "}: NOT exists.");
            }
        }
        return column_mapping;
    }

    template<class T>
    void handle(T& handler, xlsx::Sheet& sheet, std::vector<int>& column_mapping) {
        if (config.handler.comment_row != boost::none) {
            int row = config.handler.comment_row.value() - 1;
            handler.begin_comment_row();
            for (int k = 0; k < column_mapping.size(); ++k) {
                auto& field = config.fields[k];
                if (field.type == YamlConfig::Field::Type::kIsIgnored) continue;
                auto i = column_mapping[k];
                if (i == -1) {
                    handler.field(field, std::string());
                } else {
                    auto& cell = sheet.cell(row, i);
                    handler.field(field, cell.as_str());
                }
            }
            handler.end_comment_row();
        }
        for (int j = config.row; j < sheet.nrows(); ++j) {
            bool is_empty_line = true;
            bool is_ignored = false;
            for (int k = 0; k < column_mapping.size(); ++k) {
                using CT = xlsx::Cell::Type;
                auto& field = config.fields[k];
                auto i = column_mapping[k];
                auto& cell = sheet.cell(j, i);
                if (i == -1) continue;
                if (cell.type != CT::kEmpty) {
                    is_empty_line = false;
                }
                if (field.type == YamlConfig::Field::Type::kIsIgnored) {
                    if (cell.type == CT::kInt || cell.type == CT::kDouble) {
                        is_ignored = cell.as_int() != 0;
                    }
                    if (cell.type == CT::kString) {
                        is_ignored = truthy(cell.as_str());
                    }
                    if (is_ignored) break;
                }
            }
            if (is_empty_line || is_ignored) { continue; }

            handler.begin_row();
            for (int k = 0; k < column_mapping.size(); ++k) {
                auto& field = config.fields[k];
                if (field.type == YamlConfig::Field::Type::kIsIgnored) continue;
                auto i = column_mapping[k];
                if (i == -1) {;
                    if (!field.using_default) {
                        throw EXCEPTION("optional field requires default.");
                    }
                    handle_cell_default(handler, field);
                } else {
                    auto& cell = sheet.cell(j, i);
                    auto& validator = validators[k];
                    auto& relation = relations[k];
                    try {
                        handle_cell(handler, cell, field, validator, relation);
                    } catch (std::exception& exc) {
                        throw EXCEPTION("field=", field.column, ": cell[", cell.cellname(), "]=",
                                        "{value=", cell.as_str(), ",type=", cell.type_name(), "}: ",
                                        exc.what());
                    }
                }
            }
            try {
                handler.end_row();
            } catch (std::exception& exc) {
                throw EXCEPTION("row=", j, ": ", exc.what());
            }
        }
    }

    template<class T>
    void handle_cell(T& handler, xlsx::Cell& cell, YamlConfig::Field& field,
                     boost::optional<Validator>& validator,
                     boost::optional<handlers::RelationMap&>& relation) {
        using FT = YamlConfig::Field::Type;
        using CT = xlsx::Cell::Type;

        switch (field.type) {
            case FT::kInt : {
                if (field.definition != boost::none) {
                    auto it = field.definition->find(cell.as_str());
                    if (it == field.definition->end()) {
                        throw EXCEPTION("not in definition.");
                    }
                    int64_t v = std::stoi(it->second);
                    if (validator != boost::none) validator.value()(v);
                    handler.field(field, v);
                    return;
                }
                if (cell.type == CT::kInt || cell.type == CT::kDouble) {
                    auto v = cell.as_int();
                    if (validator != boost::none) validator.value()(v);
                    handler.field(field, v);
                    return;
                }
                if (cell.type == CT::kEmpty && field.using_default) {
                    handle_cell_default(handler, field);
                    return;
                }
                throw EXCEPTION("type error. expect int.");
            }
            case FT::kFloat: {
                if (field.definition != boost::none) {
                    auto it = field.definition->find(cell.as_str());
                    if (it == field.definition->end()) {
                        throw EXCEPTION("not in definition.");
                    }
                    double v = std::stod(it->second);
                    handler.field(field, v);
                    return;
                }
                if (cell.type == CT::kInt || cell.type == CT::kDouble) {
                    handler.field(field, cell.as_double());
                    return;
                }
                if (cell.type == CT::kEmpty && field.using_default) {
                    handle_cell_default(handler, field);
                    return;
                }
                throw EXCEPTION("type error. expect float.");
            }
            case FT::kBool: {
                if (field.definition != boost::none) {
                    auto it = field.definition->find(cell.as_str());
                    if (it == field.definition->end()) {
                        throw EXCEPTION("not in definition.");
                    }
                    auto s = it->second;
                    bool v = s != "false" && s != "no";
                    handler.field(field, v);
                    return;
                }
                if (cell.type == CT::kEmpty && field.using_default) {
                    handle_cell_default(handler, field);
                    return;
                }
                if (cell.type == CT::kEmpty) {
                    handler.field(field, false);
                    return;
                }
                if (cell.type == CT::kInt || cell.type == CT::kDouble) {
                    handler.field(field, cell.as_int() != 0);
                    return;
                }
                if (cell.type == CT::kString) {
                    auto v = truthy(cell.as_str());
                    handler.field(field, v);
                    return;
                }
                throw EXCEPTION("type error. expect bool.");
            }
            case FT::kChar: {
                if (field.definition != boost::none) {
                    auto it = field.definition->find(cell.as_str());
                    if (it == field.definition->end()) {
                        throw EXCEPTION("not in definition.");
                    }
                    handler.field(field, it->second);
                    return;
                }
                if (cell.type == CT::kEmpty && field.using_default) {
                    handle_cell_default(handler, field);
                    return;
                }
                auto v = cell.as_str();
                if (validator != boost::none) validator.value()(v);
                handler.field(field, v);
                return;
            }
            case FT::kDateTime: {
                if (field.definition != boost::none) {
                    throw EXCEPTION("not support datetime definition.");
                    return;
                }
                auto tz = config.arg_config.tz_seconds;
                if (cell.type == CT::kDateTime) {
                    auto time = cell.as_time64(tz);
                    handler.field(field, utils::dateutil::isoformat64(time, tz));
                    return;
                }
                if (cell.type == CT::kEmpty && field.using_default) {
                    handle_cell_default(handler, field);
                    return;
                }
                if (cell.type == CT::kString) {
                    auto time = utils::dateutil::parse64(cell.as_str(), tz);
                    if (time == utils::dateutil::ntime) {
                        throw EXCEPTION("parsing datetime error.");
                    }
                    handler.field(field, utils::dateutil::isoformat64(time, tz));
                    return;
                }
                throw EXCEPTION("type error. expect datetime.");
            }
            case FT::kUnixTime: {
                if (field.definition != boost::none) {
                    throw EXCEPTION("not support unixtime definition.");
                    return;
                }
                auto tz = config.arg_config.tz_seconds;
                if (cell.type == CT::kDateTime) {
                    auto time = cell.as_time64(tz);
                    handler.field(field, time);
                    return;
                }
                if (cell.type == CT::kEmpty && field.using_default) {
                    handle_cell_default(handler, field);
                    return;
                }
                if (cell.type == CT::kString) {
                    auto time = utils::dateutil::parse64(cell.as_str(), tz);
                    if (time == utils::dateutil::ntime) {
                        throw EXCEPTION("parsing datetime error.");
                    }
                    handler.field(field, time);
                    return;
                }
                throw EXCEPTION("type error. expect datetime.");
            }
            case FT::kForeignKey: {
                if (ignore_relation) return;
                if (field.definition != boost::none) {
                    throw EXCEPTION("not support foreignkey definition.");
                    return;
                }
                if (relation == boost::none) {
                    throw EXCEPTION("requires relation map.");
                }
                if (cell.type == CT::kEmpty && field.using_default) {
                    handle_cell_default(handler, field);
                    return;
                }
                auto& relmap = relation.value();
                if (relmap.id != field.relation->id) {
                    throw EXCEPTION("relation maps was broken. id=", relmap.id);
                }

                #define RELATION_TYPE_EXCEPTION() \
                    EXCEPTION("invalid relation type pair ", relmap.key_type_name, " -> ", \
                              relmap.column_type_name)
                if (relmap.key_type == FT::kChar) {;
                    if (relmap.column_type == FT::kInt) {
                        try {
                            auto v = relmap.get<int64_t, std::string>(cell.as_str());
                            if (validator != boost::none) validator.value()(v);
                            handler.field(field, v);
                        } catch (std::exception& exc) {
                            throw EXCEPTION(exc.what());
                        }
                        return;
                    } else {
                        throw RELATION_TYPE_EXCEPTION();
                    }
                } else if (relmap.key_type == FT::kInt) {
                    if (cell.type != CT::kInt && cell.type != CT::kDouble) {
                        throw EXCEPTION("not matched relation key_type.");
                    }
                    if (relmap.column_type == FT::kInt) {
                        try {
                            auto v = relmap.get<int64_t, int64_t>(cell.as_int());
                            if (validator != boost::none) validator.value()(v);
                            handler.field(field, v);
                        } catch (utils::exception& exc) {
                            throw EXCEPTION(exc.what());
                        }
                        return;
                    } else {
                        throw RELATION_TYPE_EXCEPTION();
                    }
                } else {
                    throw RELATION_TYPE_EXCEPTION();
                }
                #undef RELATION_TYPE_EXCEPTION
            }
            case FT::kError: {
                throw EXCEPTION("field.type error.");
            }
            case FT::kIsIgnored: {
                return;
            }
        }
        throw EXCEPTION("unknown field error.");
    }

    template<class T>
    void handle_cell_default(T& handler, YamlConfig::Field& field) {
        auto& v = field.default_value;
        if (v.type() == typeid(int64_t)) {
            handler.field(field, boost::any_cast<int64_t>(v));
        } else if (v.type() == typeid(double)) {
            handler.field(field, boost::any_cast<double>(v));
        } else if (v.type() == typeid(bool)) {
            handler.field(field, boost::any_cast<bool>(v));
        } else if (v.type() == typeid(std::string)) {
            handler.field(field, boost::any_cast<std::string>(v));
        } else if (v.type() == typeid(std::nullptr_t)) {
            handler.field(field, boost::any_cast<std::nullptr_t>(v));
        }
    }
};

}  // namespace xlsxconverter
#undef EXCEPTION
