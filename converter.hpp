#pragma once
#include <iostream>

#include "xlsx.hpp"
#include "yaml_config.hpp"
#include "arg_config.hpp"

#include "writers.hpp"
#include "utils.hpp"

namespace xlsxconverter {

struct Converter
{
    struct Validator {
        Field& field;
        std::unordered_set<std::string> strdata;
        std::unordered_set<int64_t> intdata;

        inline Validator(Field& field) : field(field) {}

        inline bool operator(std::string& val) {
            if (!field.validate.unique) return true;
            if (strdata.count(val) != 0) return false;
            strdata.insert(val);
            return true;
        }
        inline bool operator(int64_t& val) {
            if (!field.validate.unique) return true;
            if (intdata.count(val) != 0) return false;
            intdata.insert(val);
            return true;
        }
    }

    YamlConfig& config;
    std::vector<Validator> validators;

    inline
    Converter(YamlConfig& config_) : config(config_) {
        for (auto& field: config.fields) {
            validators.push_emplace(field);
        }
    }

    inline
    void run() {
        using namespace xlsx;

        if (!config.arg_config.quiet) {
            utils::log("target_sheet: %s", config.target_sheet_name);
        }
        if (!config.arg_config.quiet) {
            utils::log("target_xls: %s", config.target_xls_path);
        }
        auto book = Workbook(config.get_xls_path());
        auto& sheet = book.sheet_by_name(config.target_sheet_name);

        auto column_mapping = map_column(sheet);

        // process data
        std::string out;
        if (config.handler.type == YamlConfig::Handler::Type::kJson) {
            out = write<writers::JsonWriter>(sheet, column_mapping);
        } else {
            throw utils::exception("unknown handler.type=%d.", config.handler.type);
        }

        auto fo = std::ofstream(config.get_output_path().c_str(), std::ios::binary);
        fo << out;
        if (!config.arg_config.quiet) {
            utils::log("%s writed.", config.handler.path);
        }
    }

    inline
    std::vector<int> map_column(xlsx::Sheet& sheet) {
        std::vector<int> column_mapping;
        for (int k = 0; k < config.fields.size(); ++k) {
            auto& field = config.fields[k];
            bool found = false;
            // utils::log("i=%d cell=%s type=%d", i, cell.as_str(), cell.type);
            for (int i = 0; i < sheet.ncols(); ++i) {;
                auto& cell = sheet.cell(config.row - 1, i);
                // utils::log("name=%s cell=%s", field.name, cell.as_str());
                if (cell.as_str() == field.name) {
                    if (utils::contains(column_mapping, k)) {
                        throw utils::exception("%s: field.column=%s is duplicated.",
                                               config.target, field.column);
                    }
                    column_mapping.push_back(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw utils::exception("%s: field{column=%s,name=%s} is NOT exists in row=%d.", 
                                       config.target, field.column, field.name, config.row);
            }
        }
        return column_mapping;
    }

    template<class T>
    std::string write(xlsx::Sheet& sheet, std::vector<int>& column_mapping) {
        auto writer = T(config);
        writer.begin();
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

            writer.begin_row();
            for (int k = 0; k < column_mapping.size(); ++k) {
                auto& field = config.fields[k];
                auto i = column_mapping[k];
                auto& cell = sheet.cell(j, i);
                write_cell(writer, cell, field, k);
            }
            writer.end_row();
        }
        writer.end();
        return writer.buffer.str();
    }

    template<class T>
    void write_cell(T& writer, xlsx::Cell& cell, YamlConfig::Field& field, int index) {
        using FT = YamlConfig::Field::Type;
        using CT = xlsx::Cell::Type;

        if (field.type == FT::kInt)
        {
            if (cell.type == CT::kInt || cell.type == CT::kDouble) {
                auto v = cell.as_int();
                if (!validator[index](v)) {
                    throw utils::exception("%s: cell(%d,%d)=%ld: validation error.", 
                                           config.target, cell.row, cell.col, v);
                }
                writer.field(field.column, v);
                return;
            }
            if (cell.type == CT::kEmpty && field.using_default) {
                write_cell_default(writer, cell, field);
                return;
            }
            throw utils::exception("%s: type error. cell.type=%s", config.target, cell.type_name());
        }
        else if (field.type == FT::kFloat)
        {
            if (cell.type == CT::kInt || cell.type == CT::kDouble) {
                writer.field(field.column, cell.as_double());
                return;
            }
            if (cell.type == CT::kEmpty && field.using_default) {
                write_cell_default(writer, cell, field);
                return;
            }
            throw utils::exception("%s: type error. cell.type=%s", config.target, cell.type_name());
        }
        else if (field.type == FT::kChar)
        {
            if (cell.type == CT::kEmpty && field.using_default) {
                write_cell_default(writer, cell, field);
                return;
            }
            auto v = cell.as_str();
            if (!validator[index](v)) {
                throw utils::exception("%s: cell(%d,%d)=%s: validation error.", 
                                       config.target, cell.row, cell.col, v);
            }
            writer.field(field.column, v);
            return;
        }
        else if (field.type == FT::kDateTime)
        {
            auto tz = config.arg_config.tz_seconds;
            if (cell.type == CT::kDateTime) {
                auto time = cell.as_time(tz);
                writer.field(field.column, utils::dateutil::isoformat(time, tz));
                return;
            }
            if (cell.type == CT::kEmpty && field.using_default) {
                write_cell_default(writer, cell, field);
                return;
            }
            if (cell.type == CT::kString) {
                auto time = utils::dateutil::parse(cell.as_str(), tz);
                if (time == utils::dateutil::ntime) {
                    throw utils::exception("%s: date parse error. cell(%d,%d)=[%s]", 
                                           config.target, cell.row, cell.col, cell.as_str());
                }
                writer.field(field.column, utils::dateutil::isoformat(time, tz));
                return;
            }
            throw utils::exception("%s: type error. expected_type=datetime cell(%d,%d).type=%s", 
                                   config.target, cell.row, cell.col, cell.type_name());
        }
        throw utils::exception("%s: unknown field.type=%d", field.type);
    }

    template<class T>
    void write_cell_default(T& writer, xlsx::Cell& cell, YamlConfig::Field& field) {
        auto& v = field.default_value;
        if (v.type() == typeid(int64_t)) {
            writer.field(field.column, boost::any_cast<int64_t>(v));
        } else if (v.type() == typeid(double)) {
            writer.field(field.column, boost::any_cast<double>(v));
        } else if (v.type() == typeid(bool)) {
            writer.field(field.column, boost::any_cast<bool>(v));
        } else if (v.type() == typeid(std::string)) {
            writer.field(field.column, boost::any_cast<std::string>(v));
        } else if (v.type() == typeid(std::nullptr_t)) {
            writer.field(field.column, boost::any_cast<std::nullptr_t>(v));
        }
    }
    
};

}
