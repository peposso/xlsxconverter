#pragma once
#include <iostream>

#include "xlsx.hpp"
#include "yaml_config.hpp"
#include "arg_config.hpp"
#include "writers/json.hpp"
#include "util.hpp"

namespace xlsxconverter {

struct Converter
{
    YamlConfig& config;

    inline
    Converter(YamlConfig& config_) : config(config_) {}

    inline
    void run() {
        using namespace xlsx;

        auto book = Workbook(config.get_xls_path());
        auto& sheet = book.sheet_by_name(config.target_sheet_name);

        auto column_mapping = map_column(sheet);

        // process data
        std::string out;
        if (config.handler.type == YamlConfig::Handler::Type::kJson) {
            out = write<writers::JsonWriter>(sheet, column_mapping);
        } else {
            throw util::exception("unknown handler.type=%d.", config.handler.type);
        }

        auto fo = std::ofstream(config.get_output_path().c_str(), std::ios::binary);
        fo << out;

    }

    inline
    std::vector<int> map_column(xlsx::Sheet& sheet) {
        std::vector<int> column_mapping;
        for (int k = 0; k < config.fields.size(); ++k) {
            auto& field = config.fields[k];
            bool found = false;
            // util::log("i=%d cell=%s type=%d", i, cell.as_str(), cell.type);
            for (int i = 0; i < sheet.ncols(); ++i) {;
                auto& cell = sheet.cell(config.row - 1, i);
                // util::log("name=%s", field.name);
                if (cell.as_str() == field.name) {
                    if (util::contains(column_mapping, k)) {
                        throw util::exception("field.column=%s is duplicated.", field.column);
                    }
                    column_mapping.push_back(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw util::exception("field{column=%s,name=%s} is NOT exists in xlsx.", field.column, field.name);
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
                write_cell(writer, cell, field);
            }
            writer.end_row();
        }
        writer.end();
        return writer.buffer.str();
    }

    template<class T>
    void write_cell(T& writer, xlsx::Cell& cell, YamlConfig::Field& field) {
        using FT = YamlConfig::Field::Type;
        using CT = xlsx::Cell::Type;

        if (field.type == FT::kInt)
        {
            if (cell.type == CT::kInt || cell.type == CT::kDouble) {
                writer.field(field.column, cell.as_int());
                return;
            }
            if (cell.type == CT::kEmpty && field.using_default) {
                write_cell_default(writer, cell, field);
                return;
            }
            throw util::exception("%s: type error. cell.type=%s", config.target, cell.type_name());
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
            throw util::exception("%s: type error. cell.type=%s", config.target, cell.type_name());
        }
        else if (field.type == FT::kChar)
        {
            if (cell.type == CT::kEmpty && field.using_default) {
                write_cell_default(writer, cell, field);
                return;
            }
            writer.field(field.column, cell.as_str());
            return;
        }
        else if (field.type == FT::kDateTime)
        {
            if (cell.type == CT::kDateTime) {
                writer.field(field.column, cell.isoformat());
                return;
            }
            if (cell.type == CT::kEmpty && field.using_default) {
                write_cell_default(writer, cell, field);
                return;
            }
            if (cell.type == CT::kString) {
                time_t time = util::parse_datetime(cell.as_str());
                util::log("time=%ld", time);
                writer.field(field.column, util::isoformat(time));
                return;
            }
            throw util::exception("%s: type error. cell.type=%s", config.target, cell.type_name());
        }
        throw util::exception("%s: unknown field.type=%d", field.type);
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
