#pragma once

#include <stdio.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <memory>
#include <exception>
#include <time.h>

#include <ZipFile.h>
#include <pugixml.hpp>

namespace xlsx {

template<class...A>
std::string stringer(const A&...a) {
    auto ss = std::stringstream();
    (void)(int[]){0, ((void)(ss << a), 0)... };
    return ss.str();
}


struct StyleSheet
{
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

    const static std::unordered_map<std::string, bool> non_date_formats;
    const static std::string date_chars;
    const static std::string num_chars;

    inline
    StyleSheet(std::unique_ptr<pugi::xml_document> doc) {
        auto ss = doc->child("styleSheet");
        for (auto& nf: ss.child("numFmts").children("numFmt")) {
            int fmtid = nf.attribute("numFmtId").as_int();
            auto fmtcode = nf.attribute("formatCode").as_string();
            format_codes[fmtid] = fmtcode;
        }
        for (auto& xf: ss.child("cellXfs").children("xf")) {
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
        if (non_date_formats.count(code) == 1) {
            is_date_table_[xf_index] = false;
            return false;
        }

        // Heuristics
        auto s = remove_bracketed(code);
        bool got_sep = false;
        int date_count = 0;
        int num_count = 0;
        for (auto c: s) {
            if (date_chars.find(c) != std::string::npos) {
                date_count += 5;
            } else if (num_chars.find(c) != std::string::npos) {
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
        for (auto c: s) {
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
const std::unordered_map<std::string, bool> StyleSheet::non_date_formats = {
    {"0.00E+00", true},
    {"##0.0E+0", true},
    {"General", true},
    {"GENERAL", true},  // OOo Calc 1.1.4 does this.
    {"general", true},  // pyExcelerator 0.6.3 does this.
    {"@", true},
};
const std::string StyleSheet::date_chars = "ymdhs";
const std::string StyleSheet::num_chars = "0#?";


struct Cell
{
    enum Type {
        kEmpty, kString, kInt, kDouble, kDateTime
    };

    static int document_tzseconds;
    static int format_tzseconds;

    std::string v;
    int64_t i_ = -1;
    Type type = Type::kEmpty;

    inline
    Cell() : Cell("", "", -1, nullptr, nullptr) {}

    inline
    Cell(std::string v_, std::string t, int s,
         std::shared_ptr<std::vector<std::string>> shared_string,
         std::shared_ptr<StyleSheet> style_sheet)
    : v(v_) {
        if (v == "") {
            type = Type::kEmpty;
        } else if (t == "s") {
            type = Type::kString;
            if (shared_string.get() != nullptr) {
                int i = std::stoi(v_);
                v = i < shared_string->size() ? shared_string->at(i) : "";
            }
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
    time_t as_time() {
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
        // 25569 is epoch days.
        time_t time = (xldays - 25569) * 86400 + seconds;
        return time - document_tzseconds;
    }

    inline
    std::string isoformat() {
        int tz = format_tzseconds;
        auto time = as_time() + tz;
        auto tm = gmtime(&time);
        std::stringstream ss;
        ss << std::setfill('0') << std::setw(4) << (tm->tm_year + 1900) << '-';
        ss << std::setfill('0') << std::setw(2) << (tm->tm_mon + 1) << '-';
        ss << std::setfill('0') << std::setw(2) << (tm->tm_mday) << 'T';
        ss << std::setfill('0') << std::setw(2) << (tm->tm_hour) << ':';
        ss << std::setfill('0') << std::setw(2) << (tm->tm_min) << ':';
        ss << std::setfill('0') << std::setw(2) << (tm->tm_sec);
        if (tz == 0) {
            ss << 'Z';
            return ss.str();
        }
        if (tz < 0) {
            tz = -tz;
            ss << '-';
        } else {
            ss << '+';
        }
        int minutes = tz / 60;
        int hour = minutes / 60;
        int minute = minutes % 60;
        ss << std::setfill('0') << std::setw(2) << (hour);
        ss << std::setfill('0') << std::setw(2) << (minute);
        return ss.str();
    }

    std::string as_str() {
        return v;
    }
};
int Cell::document_tzseconds = 0;
int Cell::format_tzseconds = 0;

struct Sheet
{
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

    Cell empty_cell;

    Sheet() = default;

    inline
    Sheet(std::string rid_, std::string name_,
          std::unique_ptr<pugi::xml_document> doc_,
          std::shared_ptr<std::vector<std::string>> shared_string_,
          std::shared_ptr<StyleSheet> style_sheet_)
    : rid(rid_), name(name_), doc(std::move(doc_)), 
      shared_string(shared_string_), style_sheet(style_sheet_)
    {}

    inline
    std::unordered_map<int, pugi::xml_node>& row_nodes() {
        if (!row_nodes_.empty())
            return row_nodes_;
        for(auto& row: doc->child("worksheet").child("sheetData").children("row")) {
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
        for (auto& pair: row_nodes()) {
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
        for (auto& pair: row_nodes()) {
            int cols = 0;
            for (auto& col: pair.second.children("c")) {
                cols++;
            }
            if (cols > max) max = cols;
        }
        ncols_ = max + 1;
        return ncols_;
    }

    inline
    Cell& cell(int row, int col) {
        if (row < 0 || nrows() <= row) return empty_cell;
        if (col < 0 || ncols() <= col) return empty_cell;
        if (cells_.size() <= row) cells_.resize(row + 1);
        if (col < cells_[row].size()) {
            return cells_[row][col];
        }

        auto& row_cells = cells_[row];
        row_cells.clear();
        for (auto& c: row_nodes()[row].children("c")) {
            std::string t = c.attribute("t").as_string();
            auto s = c.attribute("s").as_int();
            std::string v = c.child("v").text().as_string();
            row_cells.push_back(Cell(v, t, s, shared_string, style_sheet));
        }
        for (int i = row_cells.size(); i < ncols(); ++i) {
            row_cells.push_back(empty_cell);
        }
        return row_cells[col];
    }
};


struct Workbook
{
    struct Exception: std::runtime_error {
        Exception(const char* what) :runtime_error(what) {}
    };

    ZipArchive::Ptr archive;
    std::unordered_map<std::string, int> entry_indexes;
    std::unordered_map<std::string, std::string> rels;
    int nsheets_ = -1;

    std::unordered_map<std::string, std::string> sheet_rid_by_name;
    std::unordered_map<std::string, std::string> sheet_name_by_rid;
    std::unordered_map<std::string, Sheet> sheets;
    std::shared_ptr<std::vector<std::string>> shared_string;
    std::shared_ptr<StyleSheet> style_sheet;

    static const std::string sheet_path_prefix;  // = "xl/worksheets/sheet";

    Workbook() = delete;

    inline
    Workbook(std::string filename)
    : shared_string(new std::vector<std::string>())
    {
        archive = ZipFile::Open(filename);

        int max_sheet_id = -1;
        size_t count = archive->GetEntriesCount();
        std::vector<int> rel_entries;
        for (size_t i = 0; i < count; ++i)
        {
            auto entry = archive->GetEntry(int(i));
            std::string fullname = entry->GetFullName();
            auto p = fullname.rfind('.');
            if (p == std::string::npos) continue;
            auto ext = fullname.substr(p);
            if (ext == ".xml" ) {
            } else if (ext == ".rels") {
                rel_entries.push_back(i);
            } else {
                continue;
            }
            entry_indexes[fullname] = i;
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

        for (int i: rel_entries) {
            auto doc = load_doc(i);
            for (auto rel: doc->child("Relationships").children("Relationship")) {
                auto rid = rel.attribute("Id").as_string();
                std::string target = rel.attribute("Target").as_string();
                if (target.substr(0, 3) != "xl/") {
                    target = "xl/" + target;
                }
                rels[rid] = target;
            }
        }

        {
            auto doc = load_doc("xl/workbook.xml");
            auto sheets = doc->child("workbook").child("sheets");
            for (auto sheet: sheets.children("sheet")) {
                // auto sheet_id = sheet.attribute("sheetId").as_int();
                auto sheet_rid = sheet.attribute("r:id").as_string();
                auto sheet_name = sheet.attribute("name").as_string();
                sheet_rid_by_name[sheet_name] = sheet_rid;
                sheet_name_by_rid[sheet_rid] = sheet_name;
            }
        }

        {
            auto doc = load_doc("xl/sharedStrings.xml");
            auto sheets = doc->child("sst");
            for (auto si: sheets.children("si")) {
                auto text = si.child("t").text().as_string();
                shared_string->push_back(text);
            }
        }

        {
            style_sheet = std::shared_ptr<StyleSheet>(
                            new StyleSheet(load_doc("xl/styles.xml")));
        }
        set_localtz();
    }

    inline
    std::string read_entry(int index) {
        auto entry = archive->GetEntry(index);
        auto stream_ptr = entry->GetDecompressionStream();
        if (stream_ptr == nullptr) {
            throw Exception("cant decode stream !!");
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
            throw Exception(stringer("entry=", name, " not found").c_str());
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
            throw Exception("unknown sheet name");
        }
        return sheet(sheet_rid_by_name[name]);
    }

    inline
    void set_document_tz(int seconds) {
        Cell::document_tzseconds = seconds;
    }

    inline
    void set_format_tz(int seconds) {
        Cell::format_tzseconds = seconds;
    }

    inline
    void set_localtz() {
        time_t local_time;
        time(&local_time);
        auto utc_tm = *gmtime(&local_time);
        time_t utc_time = mktime(&utc_tm);
        set_document_tz(local_time - utc_time);
        set_format_tz(local_time - utc_time);
    }
};

const std::string Workbook::sheet_path_prefix = "xl/worksheets/sheet";

}
