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

        // mapping column.
        std::vector<int> column_order;
        for (int k = 0; k < config.fields.size(); ++k) {
            auto& field = config.fields[k];
            bool found = false;
            // util::log("i=%d cell=%s type=%d", i, cell.as_str(), cell.type);
            for (int i = 0; i < sheet.ncols(); ++i) {;
                auto& cell = sheet.cell(config.row - 1, i);
                // util::log("name=%s", field.name);
                if (cell.as_str() == field.name) {
                    if (util::contains(column_order, k)) {
                        throw util::exception("field.column=%s is duplicated.", field.column);
                    }
                    column_order.push_back(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw util::exception("field{column=%s,name=%s} is NOT exists in xlsx.", field.column, field.name);
            }
        }

        // process data
        std::string out;
        if (config.handler.type == YamlConfig::Handler::Type::kJson) {
            out = write<writers::JsonWriter>(sheet, column_order);
        } else {
            throw util::exception("unknown handler.type=%d.", config.handler.type);
        }

        auto fo = std::ofstream(config.get_output_path().c_str(), std::ios::binary);
        fo << out;

    }

    template<class T>
    std::string write(xlsx::Sheet& sheet, std::vector<int>& column_order) {
        auto writer = T();
        writer.begin();
        for (int j = config.row; j < sheet.nrows(); ++j) {
            bool is_empty_line = true;
            for (int i: column_order) {
                auto& cell = sheet.cell(j, i);
                if (cell.type != xlsx::Cell::Type::kEmpty) {
                    is_empty_line = false;
                    break;
                }
            }
            if (is_empty_line) { continue; }

            writer.begin_row();
            for (int k = 0; k < column_order.size(); ++k) {
                auto& field = config.fields[k];
                auto i = column_order[k];
                auto& cell = sheet.cell(j, i);
                using FT = YamlConfig::Field::Type;
                using CT = xlsx::Cell::Type;

                if (field.type == FT::kInt) {
                    writer.field(field.column, cell.as_int());
                } else if (field.type == FT::kFloat) {
                    writer.field(field.column, cell.as_double());
                } else if (field.type == FT::kChar) {
                    writer.field(field.column, cell.as_str());
                } else if (field.type == FT::kDateTime) {
                    if (cell.type == CT::kDateTime) {
                        writer.field(field.column, cell.isoformat());
                    } else if (cell.type == CT::kEmpty) {
                        writer.field(field.column, nullptr);
                    } else {
                        writer.field(field.column, cell.as_str());
                        // throw util::exception("type error at cell(%d,%d)=[%s] type=%s", j, i, cell.as_str(), cell.type_name());
                    }
                }

            }
            writer.end_row();
        }
        writer.end();
        return writer.buffer.str();
    }

};

}
