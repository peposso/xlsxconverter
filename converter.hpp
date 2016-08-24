#pragma once
#include <iostream>

#include "xlsx.hpp"
#include "yaml_config.hpp"
#include "arg_config.hpp"

#include "handlers.hpp"
#include "utils.hpp"

#define EXCEPTION XLSXCONVERTER_UTILS_EXCEPTION

namespace xlsxconverter {

struct Converter
{
    struct Validator {
        YamlConfig::Field& field;
        std::unordered_set<std::string> strset;
        std::unordered_set<int64_t> intset;

        inline Validator(YamlConfig::Field& field) : field(field) {}

        inline bool operator()(std::string& val) {
            if (!field.validate) return true;
            if (field.validate->unique) {
                if (strset.count(val) != 0) return false;
                strset.insert(val);
            }
            return true;
        }
        inline bool operator()(int64_t& val) {
            if (!field.validate) return true;
            if (field.validate->unique) {
                if (intset.count(val) != 0) return false;
                intset.insert(val);
            }
            return true;
        }
    };

    YamlConfig& config;
    bool ignore_relation = false;
    std::vector<boost::optional<Validator>> validators;
    std::vector<boost::optional<handlers::RelationMap&>> relations;

    inline
    Converter(YamlConfig& config_, bool ignore_relation_=false)
        : config(config_),
          ignore_relation(ignore_relation_)
    {
        for (auto& field: config.fields) {
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
            auto book = xlsx::Workbook(xls_path);
            auto& sheet = book.sheet_by_name(config.target_sheet_name);
            auto column_mapping = map_column(sheet);
            // process data
            try {
                handle(handler, sheet, column_mapping);
            } catch (utils::exception& exc) {
                throw EXCEPTION(config.path, ": ", xls_path, ": ", exc.what());
            }
        }
        handler.end();
    }

    inline
    std::vector<int> map_column(xlsx::Sheet& sheet) {
        std::vector<int> column_mapping;
        for (int k = 0; k < config.fields.size(); ++k) {
            auto& field = config.fields[k];
            bool found = false;
            for (int i = 0; i < sheet.ncols(); ++i) {;
                auto& cell = sheet.cell(config.row - 1, i);
                // utils::log("i=", i, " name=",field.name, " cell=", cell.as_str());
                if (cell.as_str() == field.name) {
                    // if (utils::contains(column_mapping, i)) {
                    //     throw EXCEPTION(config.target, ": field=", field.column, ": duplicated.");
                    // }
                    column_mapping.push_back(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                for (int i = 0; i < sheet.ncols(); ++i) {;
                    utils::log("cell(", config.row-1, ",", i, ")=", sheet.cell(config.row-1, i).as_str());
                }
                throw EXCEPTION("row=", config.row, ": field{column=",
                                field.column, ",name=", field.name, "}: NOT exists.");
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
                auto i = column_mapping[k];
                auto& cell = sheet.cell(row, i);
                handler.field(field, cell.as_str());
            }
            handler.end_comment_row();
        }
        for (int j = config.row; j < sheet.nrows(); ++j) {
            bool is_empty_line = true;
            for (int i: column_mapping) {
                auto& cell = sheet.cell(j, i);
                if (cell.type != xlsx::Cell::Type::kEmpty) {
                    is_empty_line = false;
                    break;
                }
            }
            if (is_empty_line) { continue; }

            handler.begin_row();
            for (int k = 0; k < column_mapping.size(); ++k) {
                auto& field = config.fields[k];
                auto i = column_mapping[k];
                auto& cell = sheet.cell(j, i);
                auto& validator = validators[k];
                auto& relation = relations[k];
                handle_cell(handler, cell, field, validator, relation);
            }
            try {
                handler.end_row();
            } catch (utils::exception& exc) {
                throw EXCEPTION(config.path, ": ", config.target, ": row=", j, ": ", exc.what());
            }
        }
    }

    template<class T>
    void handle_cell(T& handler, xlsx::Cell& cell, YamlConfig::Field& field, boost::optional<Validator>& validator, boost::optional<handlers::RelationMap&>& relation) {
        using FT = YamlConfig::Field::Type;
        using CT = xlsx::Cell::Type;

        #define EXCEPT(...) EXCEPTION("field=", field.column, ": cell(", cell.row, ",", cell.col, \
                                      "){value=", cell.as_str(), ",type=", cell.type_name(), "}: ", __VA_ARGS__)

        if (field.type == FT::kInt)
        {
            if (field.definition != boost::none) {
                auto it = field.definition->find(cell.as_str());
                if (it == field.definition->end()) {
                    throw EXCEPT("not in definition.");
                }
                int64_t v = std::stoi(it->second);
                handler.field(field, v);
                return;
            }
            if (cell.type == CT::kInt || cell.type == CT::kDouble) {
                auto v = cell.as_int();
                if (validator != boost::none && !validator.value()(v)) {
                    throw EXCEPT("validation error.");
                }
                handler.field(field, v);
                return;
            }
            if (cell.type == CT::kEmpty && field.using_default) {
                handle_cell_default(handler, cell, field);
                return;
            }
            throw EXCEPT("type error. expect int.");
        }
        else if (field.type == FT::kFloat)
        {
            if (field.definition != boost::none) {
                auto it = field.definition->find(cell.as_str());
                if (it == field.definition->end()) {
                    throw EXCEPT("not in definition.");
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
                handle_cell_default(handler, cell, field);
                return;
            }
            throw EXCEPT("type error. expect float.");
        }
        else if (field.type == FT::kBool)
        {
            if (field.definition != boost::none) {
                auto it = field.definition->find(cell.as_str());
                if (it == field.definition->end()) {
                    throw EXCEPT("not in definition.");
                }
                auto s = it->second;
                bool v = s != "false" && s != "no";
                handler.field(field, v);
                return;
            }
            if (cell.type == CT::kEmpty && field.using_default) {
                handle_cell_default(handler, cell, field);
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
                auto s = cell.as_str();
                if (s == "False" || s == "false" || s == "FALSE" || s == "No" || s == "no" || s == "NO" || s == "0" || s == "0.0") {
                    handler.field(field, false);
                } else {
                    handler.field(field, true);
                }
                return;
            }
            throw EXCEPT("type error. expect bool.");
        }
        else if (field.type == FT::kChar)
        {
            if (field.definition != boost::none) {
                auto it = field.definition->find(cell.as_str());
                if (it == field.definition->end()) {
                    throw EXCEPT("not in definition.");
                }
                handler.field(field, it->second);
                return;
            }
            if (cell.type == CT::kEmpty && field.using_default) {
                handle_cell_default(handler, cell, field);
                return;
            }
            auto v = cell.as_str();
            if (validator != boost::none && !validator.value()(v)) {
                throw EXCEPT("validation error.");
            }
            handler.field(field, v);
            return;
        }
        else if (field.type == FT::kDateTime)
        {
            if (field.definition != boost::none) {
                throw EXCEPT("not support datetime definition.");
                return;
            }
            auto tz = config.arg_config.tz_seconds;
            if (cell.type == CT::kDateTime) {
                auto time = cell.as_time64(tz);
                handler.field(field, utils::dateutil::isoformat64(time, tz));
                return;
            }
            if (cell.type == CT::kEmpty && field.using_default) {
                handle_cell_default(handler, cell, field);
                return;
            }
            if (cell.type == CT::kString) {
                auto time = utils::dateutil::parse64(cell.as_str(), tz);
                if (time == utils::dateutil::ntime) {
                    throw EXCEPT("parsing datetime error.");
                }
                handler.field(field, utils::dateutil::isoformat64(time, tz));
                return;
            }
            throw EXCEPT("type error. expect datetime.");
        }
        else if (field.type == FT::kUnixTime)
        {
            if (field.definition != boost::none) {
                throw EXCEPT("not support unixtime definition.");
                return;
            }
            auto tz = config.arg_config.tz_seconds;
            if (cell.type == CT::kDateTime) {
                auto time = cell.as_time64(tz);
                handler.field(field, time);
                return;
            }
            if (cell.type == CT::kEmpty && field.using_default) {
                handle_cell_default(handler, cell, field);
                return;
            }
            if (cell.type == CT::kString) {
                auto time = utils::dateutil::parse64(cell.as_str(), tz);
                if (time == utils::dateutil::ntime) {
                    throw EXCEPT("parsing datetime error.");
                }
                handler.field(field, time);
                return;
            }
            throw EXCEPT("type error. expect datetime.");
        }
        else if (field.type == FT::kForeignKey)
        {
            if (ignore_relation) return;
            if (field.definition != boost::none) {
                throw EXCEPT("not support foreignkey definition.");
                return;
            }
            if (relation == boost::none) {
                throw EXCEPT("requires relation map.");
            }
            if (cell.type == CT::kEmpty && field.using_default) {
                handle_cell_default(handler, cell, field);
                return;
            }
            auto& relmap = relation.value();
            if (relmap.id != field.relation->id) {
                throw EXCEPT("relation maps was broken. id=", relmap.id);
            }

            if (relmap.key_type == FT::kChar) {;
                if (relmap.column_type == FT::kInt) {
                    try {
                        auto v = relmap.get<int64_t, std::string>(cell.as_str());
                        handler.field(field, v);
                    } catch (utils::exception& exc) {
                        throw EXCEPT(exc.what());
                    }
                    return;
                } else {
                    throw EXCEPT("usable relation type maps are (char -> int), (int -> int).");
                }
            } else if (relmap.key_type == FT::kInt) {
                if (cell.type != CT::kInt && cell.type != CT::kDouble) {
                    throw EXCEPT("not matched relation key_type.");
                }
                if (relmap.column_type == FT::kInt) {
                    try {
                        auto v = relmap.get<int64_t, int64_t>(cell.as_int());
                        handler.field(field, v);
                    } catch (utils::exception& exc) {
                        throw EXCEPT(exc.what());
                    }
                    return;
                } else {
                    throw EXCEPT("usable relation type maps are (char -> int), (int -> int).");
                }
            } else {
                throw EXCEPT("usable relation type maps are (char -> int), (int -> int). (",
                             relmap.key_type_name, " -> ", relmap.column_type_name, ")");
            }
        }

        throw EXCEPT("unknown field error.");
        #undef EXCEPT
    }

    template<class T>
    void handle_cell_default(T& handler, xlsx::Cell& cell, YamlConfig::Field& field) {
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

}
