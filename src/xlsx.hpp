// Copyright (c) 2016 peposso All Rights Reserved.
// Released under the MIT license
#pragma once
#include <sys/stat.h>  // NOLINT
#include <tuple>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <memory>
#include <exception>
#include <utility>

#include <ZipFile.h>  // NOLINT
#include <pugixml.hpp>  // NOLINT

namespace xlsx {

template<class T>
void sscat_detail_(std::stringstream& ss, const T& t) {
    ss << t;
}

template<class T, class...A>
void sscat_detail_(std::stringstream& ss, const T& t, const A&...a) {
    ss << t;
    sscat_detail_(ss, a...);
}

template<class...A>
std::string sscat(const A&...a) {
    auto ss = std::stringstream();
    sscat_detail_(ss, a...);
    return ss.str();
}

struct Exception: std::runtime_error {
    template<class...A>
    inline explicit Exception(A...a) : std::runtime_error(sscat(a...).c_str()) {}
};


struct StyleSheet {
    std::vector<int> num_fmts_by_xf_index;
    std::unordered_map<int, std::string> format_codes;

    std::unordered_map<int, bool> is_date_table_;
    /*
    numFmts.
    # "std" == "standard for US English locale"
    # #### TODO ... a lot of work to tailor these to the user's locale.
    # See e.g. gnumeric-1.x.y/src/formats.c
    0x00: "General",
    0x01: "0",
    0x02: "0.00",
    0x03: "#,##0",
    0x04: "#,##0.00",
    0x05: "$#,##0_);($#,##0)",
    0x06: "$#,##0_);[Red]($#,##0)",
    0x07: "$#,##0.00_);($#,##0.00)",
    0x08: "$#,##0.00_);[Red]($#,##0.00)",
    0x09: "0%",
    0x0a: "0.00%",
    0x0b: "0.00E+00",
    0x0c: "# ?/?",
    0x0d: "# ??/??",
    0x0e: "m/d/yy",
    0x0f: "d-mmm-yy",
    0x10: "d-mmm",
    0x11: "mmm-yy",
    0x12: "h:mm AM/PM",
    0x13: "h:mm:ss AM/PM",
    0x14: "h:mm",
    0x15: "h:mm:ss",
    0x16: "m/d/yy h:mm",
    0x25: "#,##0_);(#,##0)",
    0x26: "#,##0_);[Red](#,##0)",
    0x27: "#,##0.00_);(#,##0.00)",
    0x28: "#,##0.00_);[Red](#,##0.00)",
    0x29: "_(* #,##0_);_(* (#,##0);_(* \"-\"_);_(@_)",
    0x2a: "_($* #,##0_);_($* (#,##0);_($* \"-\"_);_(@_)",
    0x2b: "_(* #,##0.00_);_(* (#,##0.00);_(* \"-\"??_);_(@_)",
    0x2c: "_($* #,##0.00_);_($* (#,##0.00);_($* \"-\"??_);_(@_)",
    0x2d: "mm:ss",
    0x2e: "[h]:mm:ss",
    0x2f: "mm:ss.0",
    0x30: "##0.0E+0",
    0x31: "@",
    */

    template<class T = std::unordered_map<std::string, bool>>
    static const T& non_date_formats() {
        static const T non_date_formats = {
            {"0.00E+00", true},
            {"##0.0E+0", true},
            {"General", true},
            {"GENERAL", true},  // OOo Calc 1.1.4 does this.
            {"general", true},  // pyExcelerator 0.6.3 does this.
            {"@", true},
        };
        return non_date_formats;
    }
    inline static const std::string& date_chars() {
        static const std::string date_chars = "ymdhs";
        return date_chars;
    }
    inline static const std::string& num_chars() {
        static const std::string num_chars = "0#?";
        return num_chars;
    }

    inline
    explicit StyleSheet(std::unique_ptr<pugi::xml_document> doc) {
        auto ss = doc->child("styleSheet");
        for (auto& nf : ss.child("numFmts").children("numFmt")) {
            int fmtid = nf.attribute("numFmtId").as_int();
            auto fmtcode = nf.attribute("formatCode").as_string();
            format_codes[fmtid] = fmtcode;
        }
        for (auto& xf : ss.child("cellXfs").children("xf")) {
            int fmtid = xf.attribute("numFmtId").as_int();
            num_fmts_by_xf_index.push_back(fmtid);
        }
    }

    inline
    bool is_date_format(int xf_index) {
        // SEE: https://github.com/python-excel/xlrd/blob/master/xlrd/formatting.py
        auto it = is_date_table_.find(xf_index);
        if (it != is_date_table_.end()) return it->second;

        auto fmtid = num_fmts_by_xf_index[xf_index];
        // standard formats.
        if (0x0e <= fmtid && fmtid <= 0x16) return true;
        if (0x2d <= fmtid && fmtid <= 0x2f) return true;
        if (fmtid <= 0x31) return false;
        // 0-13: false, 14-22: true, 23-44: false, 45-47: true, 47-49: false

        auto code = format_codes[fmtid];
        if (non_date_formats().count(code) == 1) {
            is_date_table_[xf_index] = false;
            return false;
        }

        // Heuristics
        auto s = remove_bracketed(code);
        bool got_sep = false;
        int date_count = 0;
        int num_count = 0;
        for (auto c : s) {
            if (date_chars().find(c) != std::string::npos) {
                date_count += 5;
            } else if (num_chars().find(c) != std::string::npos) {
                num_count += 5;
            } else if (c == ';') {
                got_sep = true;
            }
        }
        if (date_count > 0 && num_count == 0) {
            is_date_table_[xf_index] = true;
            return true;
        }
        if (num_count > 0 && date_count == 0) {
            is_date_table_[xf_index] = false;
            return false;
        }
        bool r = date_count > num_count;
        is_date_table_[xf_index] = r;
        return r;
    }

    inline
    std::string remove_bracketed(std::string s) {
        std::string r;
        bool in_bracket = false;
        for (auto c : s) {
            if (in_bracket) {
                if (c == ']') in_bracket = false;
            } else {
                if (c == '[') {
                    in_bracket = true;
                } else {
                    r.push_back(c);
                }
            }
        }
        return r;
    }
};


struct Cell {
    enum Type {
        kEmpty, kString, kInt, kDouble, kDateTime
    };

    std::string v;
    int row = -1;
    int col = -1;
    Type type = Type::kEmpty;

    inline
    Cell() : Cell(-1, -1) {}

    inline
    Cell(int row, int col) : Cell(row, col, "", "", -1, nullptr, nullptr) {}

    inline
    Cell(int row, int col, std::string v_, std::string t, int s,
         std::shared_ptr<std::vector<std::string>> shared_string,
         std::shared_ptr<StyleSheet> style_sheet)
        : row(row), col(col), v(v_) {
        if (v == "") {
            type = Type::kEmpty;
        } else if (t == "s") {
            type = Type::kString;
            if (shared_string.get() == nullptr) {
                throw Exception("invalid shared_string: nullptr");
            }
            int i = std::stoi(v);
            if (i < 0 || shared_string->size() <= i) {
                throw Exception("invalid shared_string: invalid id=", i);
            }
            v = shared_string->at(i);
        } else {
            if (s > 0 && style_sheet->is_date_format(s)) {
                type = Type::kDateTime;
            } else if (v.find('.') != std::string::npos) {
                type = Type::kDouble;
            } else {
                type = Type::kInt;
            }
        }
    }

    inline
    std::string type_name() {
        // for debug.
        switch (type) {
            case (Type::kEmpty): return "empty";
            case (Type::kString): return "string";
            case (Type::kInt): return "int";
            case (Type::kDouble): return "double";
            case (Type::kDateTime): return "datetime";
        }
        return "error";
    }

    inline
    std::string cellname() {
        std::string name;
        int ncol = col + 1;
        while (ncol > 0) {
            if (ncol % 26 == 0) {
                name = "Z" + name;
                ncol = ncol / 26;
                ncol--;
            } else {
                name = std::string(1, 'A' + ncol % 26 - 1) + name;
                ncol = ncol / 26;
            }
        }
        return name + std::to_string(row + 1);
    }

    inline
    int64_t as_int() {
        try {
            return std::stoi(v);
        } catch (std::invalid_argument& exc) {
            return 0;
        }
    }

    inline
    double as_double() {
        try {
            return std::stod(v);
        } catch (std::invalid_argument& exc) {
            return 0.0;
        }
    }

    inline
    int64_t as_time64(int tz_seconds = 0) {
        // xldate is double.
        //   int-part: 1899-12-30 based days.
        //   frac-part: time seconds / (24*60*60).
        //   does not contain timezone info.
        auto xldatetime = as_double();
        int64_t xldays = xldatetime;
        double seconds = (xldatetime - xldays) * 86400.0;
        if (seconds == 86400.0) {
            seconds = 0.0;
            xldays += 1;
        }
        if (seconds > 0.1) {
            seconds += 0.5;
        } else if (seconds < -0.1) {
            seconds -= 0.5;
        }
        // (1970-1-1) - (1899-12-30) = 25569 days.
        return (xldays - 25569) * 86400 + seconds - tz_seconds;
    }

    inline
    time_t as_time(int tz_seconds = 0) {
        return as_time64(tz_seconds);
    }

    inline
    std::string as_str() {
        return v;
    }
};

struct Sheet {
    std::string rid;
    std::string name;
    std::unique_ptr<pugi::xml_document> doc;
    std::shared_ptr<std::vector<std::string>> shared_string;
    std::shared_ptr<StyleSheet> style_sheet;

    // cached
    std::unordered_map<int, pugi::xml_node> row_nodes_;
    int nrows_ = -1;
    int ncols_ = -1;
    std::vector<std::vector<Cell>> cells_;

    Cell ncell;

    Sheet() = default;

    inline
    Sheet(std::string rid_, std::string name_,
          std::unique_ptr<pugi::xml_document> doc_,
          std::shared_ptr<std::vector<std::string>> shared_string_,
          std::shared_ptr<StyleSheet> style_sheet_)
        : rid(rid_), name(name_),
          doc(std::move(doc_)),
          shared_string(shared_string_),
          style_sheet(style_sheet_)
    {}

    inline
    std::unordered_map<int, pugi::xml_node>& row_nodes() {
        if (!row_nodes_.empty()) {
            return row_nodes_;
        }
        for (auto& row : doc->child("worksheet").child("sheetData").children("row")) {
            int r = row.attribute("r").as_int() - 1;
            row_nodes_[r] = row;
        }
        return row_nodes_;
    }

    inline
    int nrows() {
        if (nrows_ != -1) {
            return nrows_;
        }
        int max = -1;
        for (auto& pair : row_nodes()) {
            int r = pair.first;
            if (r > max) max = r;
        }
        nrows_ = max + 1;
        return nrows_;
    }

    inline
    int ncols() {
        if (ncols_ != -1) {
            return ncols_;
        }
        int max = -1;
        for (auto& pair : row_nodes()) {
            int colmax = -1;
            for (auto& c : pair.second.children("c")) {
                std::string r = c.attribute("r").as_string();
                int colx, rowx_;
                std::tie(rowx_, colx) = parse_cellname(r);
                if (colx > colmax) colmax = colx;
            }
            if (colmax > max) max = colmax;
        }
        ncols_ = max + 1;
        return ncols_;
    }

    inline
    Cell& cell(const std::string& cellname) {
        int rowx, colx;
        std::tie(rowx, colx) = parse_cellname(cellname);
        return cell(rowx, colx);
    }

    inline
    Cell& cell(int rowx, int colx) {
        // row, col: 0-index
        if (rowx < 0 || nrows() <= rowx) return ncell;
        if (colx < 0 || ncols() <= colx) return ncell;
        if (cells_.size() <= rowx) cells_.resize(rowx + 1);
        if (colx < cells_[rowx].size()) {
            return cells_[rowx][colx];
        }

        auto& row_cells = cells_[rowx];
        row_cells.clear();
        int i = 0;
        for (auto& c : row_nodes()[rowx].children("c")) {
            std::string r = c.attribute("r").as_string();
            int colx, rowx_;
            std::tie(rowx_, colx) = parse_cellname(r);
            if (rowx_ != rowx) {
                throw Exception("bad. r=", r, " row=", rowx, " parsed_row=", rowx_);
            }
            std::string t = c.attribute("t").as_string();
            auto s = c.attribute("s").as_int();
            std::string v = c.child("v").text().as_string();
            auto cell = Cell(rowx, colx, v, t, s, shared_string, style_sheet);
            if (row_cells.size() <= colx) {
                for (int j = row_cells.size(); j < colx; ++j) {
                    // fill empty cells
                    row_cells.push_back(Cell(rowx, j));
                }
                row_cells.push_back(std::move(cell));
            } else {
                // ???
                row_cells[colx] = std::move(cell);
            }
        }
        for (int i = row_cells.size(); i < ncols(); ++i) {
            // fill empty cells
            row_cells.push_back(Cell(rowx, i));
        }
        return row_cells[colx];
    }

    inline
    std::tuple<int, int> parse_cellname(std::string r) {
        size_t p = std::string::npos;
        int colx = 0;
        for (size_t i = 0; i < r.size(); ++i) {
            uint8_t c = r[i];
            if ('A' <= c && c <= 'Z') {
                colx = colx * ('Z' - 'A' + 1) + (c - 'A' + 1);
            } else if ('0' <= c && c <= '9') {
                p = i;
                break;
            } else {
                throw Exception("bad position char. row");
            }
        }
        colx--;
        if (p == std::string::npos) {
            throw Exception("bad position char. col");
        }
        int rowx = std::stoi(r.substr(p)) - 1;
        return std::make_tuple(rowx, colx);
    }
};


struct Workbook {
    ZipArchive::Ptr archive;
    std::unordered_map<std::string, int> entry_indexes;
    std::vector<std::string> entry_names;  // for debug
    std::unordered_map<std::string, std::string> rels;
    int nsheets_ = -1;

    std::unordered_map<std::string, std::string> sheet_rid_by_name;
    std::unordered_map<std::string, std::string> sheet_name_by_rid;
    std::unordered_map<std::string, Sheet> sheets;
    std::shared_ptr<std::vector<std::string>> shared_string;
    std::shared_ptr<StyleSheet> style_sheet;

    Workbook() = delete;

    inline
    explicit Workbook(std::string filename)
            : shared_string(new std::vector<std::string>()) {
        struct stat statbuf;
        if (::stat(filename.c_str(), &statbuf) != 0) {
            throw Exception("file=", filename, " does not exist.");
        }

        archive = ZipFile::Open(filename);

        int max_sheet_id = -1;
        size_t count = archive->GetEntriesCount();
        std::vector<int> rel_entries;
        for (size_t i = 0; i < count; ++i) {
            auto entry = archive->GetEntry(static_cast<int>(i));
            std::string fullname = entry->GetFullName();
            auto p = fullname.rfind('.');
            if (p == std::string::npos) continue;
            auto ext = fullname.substr(p);
            if (ext == ".xml") {
            } else if (ext == ".rels") {
                rel_entries.push_back(i);
            } else {
                continue;
            }
            entry_indexes[fullname] = i;
            entry_names.push_back(fullname);
        }

        if (entry_indexes.count("xl/workbook.xml") == 0) {
            throw Exception("cant find xl/workbook.xml !!");
        }
        if (entry_indexes.count("xl/sharedStrings.xml") == 0) {
            throw Exception("cant find xl/sharedStrings.xml !!");
        }
        if (entry_indexes.count("xl/styles.xml") == 0) {
            throw Exception("cant find xl/styles.xml !!");
        }
        if (rel_entries.empty()) {
            throw Exception("cant find rels !!");
        }

        for (int i : rel_entries) {
            auto doc = load_doc(i);
            for (auto rel : doc->child("Relationships").children("Relationship")) {
                auto rid = rel.attribute("Id").as_string();
                std::string target = rel.attribute("Target").as_string();
                auto ext = target.substr(target.size()-4);
                if (ext != ".xml") continue;
                if (target.substr(0, 3) != "xl/") {
                    target = "xl/" + target;
                }
                rels[rid] = target;
            }
        }

        auto workbook_doc = load_doc("xl/workbook.xml");
        for (auto sheet : workbook_doc->child("workbook").child("sheets").children("sheet")) {
            // auto sheet_id = sheet.attribute("sheetId").as_int();
            auto sheet_rid = sheet.attribute("r:id").as_string();
            auto sheet_name = sheet.attribute("name").as_string();
            sheet_rid_by_name[sheet_name] = sheet_rid;
            sheet_name_by_rid[sheet_rid] = sheet_name;
        }

        auto shared_string_doc = load_doc("xl/sharedStrings.xml");
        for (auto si : shared_string_doc->child("sst").children("si")) {
            auto text = si.child("t").text().as_string();
            shared_string->push_back(text);
        }

        style_sheet = std::shared_ptr<StyleSheet>(
                        new StyleSheet(load_doc("xl/styles.xml")));
    }

    inline
    std::string read_entry(int index) {
        if (index < 0 || entry_names.size() <= index) {
            throw Exception("entry_index=", index, ": out of range.");
        }
        auto entry = archive->GetEntry(index);
        auto stream_ptr = entry->GetDecompressionStream();
        if (stream_ptr == nullptr) {
            throw Exception("entry=", entry_names[index], ": cant decode stream.");
        }
        std::stringstream ss;
        ss << stream_ptr->rdbuf();
        return ss.str();
    }

    inline
    std::unique_ptr<pugi::xml_document> load_doc(int index) {
        auto xmlsource = read_entry(index);
        // std::cerr << xmlsource << std::endl;
        auto doc = std::unique_ptr<pugi::xml_document>(new pugi::xml_document());
        auto result = doc->load_string(xmlsource.c_str());
        if (!result) {
            throw Exception("parse error!!");
        }
        return doc;
    }

    inline
    std::unique_ptr<pugi::xml_document> load_doc(const std::string& name) {
        if (entry_indexes.count(name) == 0) {
            throw Exception("entry=", name, ": not found.");
        }
        return load_doc(entry_indexes[name]);
    }

    inline int nsheets() { return nsheets_; }

    inline
    Sheet& sheet(std::string rid) {
        if (sheets.count(rid) == 1) {
            return sheets[rid];
        }
        auto entry_name = rels[rid];
        auto sheet_name = sheet_name_by_rid[rid];
        auto doc = load_doc(entry_name);
        auto sheet = Sheet(rid, sheet_name, std::move(doc), shared_string, style_sheet);
        sheets.insert(std::make_pair(rid, std::move(sheet)));
        return sheets[rid];
    }

    inline
    Sheet& sheet_by_name(std::string name) {
        if (sheet_rid_by_name.count(name) == 0) {
            throw Exception("sheet_name=", name, ": not found.");
        }
        return sheet(sheet_rid_by_name[name]);
    }
};

}  // namespace xlsx
