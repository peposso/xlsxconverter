#pragma once

#include <stdio.h>
#include <iostream>
#include <sys/stat.h>
#include <algorithm>
#include <iterator>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace xlsxconverter {
namespace util {

inline
bool fexists(const std::string& name) {
    struct stat statbuf;
    return ::stat(name.c_str(), &statbuf) == 0;
}

template<class T>
bool contains(const std::vector<T> vec, const T& value) {
    return std::find(vec.begin(), vec.end(), value) != vec.end();
}

template<class A>
struct cstr_;
template<>
struct cstr_<std::string> { const char* value; cstr_(const std::string& a) : value(a.c_str()) {} };
template<class A>
struct cstr_ { A value; cstr_(const A& a) : value(a) {} };


template<class...A>
std::string format(const std::string& fmt, const A&...a) {
    char buf[256] = {0};
    snprintf(buf, 255, fmt.c_str(), cstr_<A>(a).value...);
    buf[255] = '\0';
    return std::string(buf);
}

struct exception : public std::runtime_error
{
    template<class...A>
    exception(A...a) : std::runtime_error(format(a...).c_str()) {}
};

template<class...A>
void log(const std::string& fmt, A...a) {
    std::cerr << format(fmt, a...) << std::endl;
}


struct u8to32iter
{
    struct iterator : public std::iterator<std::input_iterator_tag, uint32_t>
    {
        size_t index = 0;
        std::string& str;
        uint32_t value;

        inline iterator(std::string& str, size_t index) : str(str), index(index) {}

        uint32_t operator*() {
            if (index >= str.size()) {
                index = str.size();
                return 0;
            }
            uint32_t c0 = str[index];
            if ((c0 & 0x80) == 0) {
                // U+0000～U+007F
                return c0;
            } else if ((c0 & 0xE0) == 0xC0) {
                // U+0080～U+07FF
                uint32_t c1 = check_(str[index+1]);
                return ((c0 & 0x1F) << 6) | (c1 & 0x3F);
            } else if ((c0 & 0xF0) == 0xE0) {
                // U+0800～U+FFFF
                uint32_t c1 = check_(str[index+1]);
                uint32_t c2 = check_(str[index+2]);
                return ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
            } else if (0xF0 <= c0 && c0 <= 0xF4) {
                // U+10000～U+10FFFF
                uint32_t c1 = check_(str[index+1]);
                uint32_t c2 = check_(str[index+2]);
                uint32_t c3 = check_(str[index+3]);
                // 上位5bit
                c0 = ((c0 & 0x7) << 2) | ((c1 >> 4) & 0x3);
                if (c0 > 0x10) {
                    throw 1;
                }
                return (c0 << 16) | ((c1 & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
            }
            throw 2;
        }
        iterator& operator++() {
            if (index >= str.size()) {
                index = str.size();
                return *this;
            }
            uint32_t c0 = str[index];
            if ((c0 & 0x80) == 0) {
                // U+0000～U+007F
                index += 1;
            } else if ((c0 & 0xE0) == 0xC0) {
                // U+0080～U+07FF
                index += 2;
            } else if ((c0 & 0xF0) == 0xE0) {
                // U+0800～U+FFFF
                index += 3;
            } else if (0xF0 <= c0 && c0 <= 0xF4) {
                // U+10000～U+10FFFF
                index += 4;
            } else {
                throw 2;
            }
            if (index > str.size()) { index = str.size(); }
            return *this;
        }
        bool operator!=(const iterator& it) {
            return index != it.index || str != it.str;
        }
        uint8_t check_(uint8_t c) {
            if ((c & 0xc0) != 0x80) { throw 0; }
            return c;
        }
    };

    std::string str;

    u8to32iter(const std::string& str) : str(str) {}
    iterator begin() { return iterator(str, 0); }
    iterator end() { return iterator(str, str.size()); }
};


struct strwalker
{
    std::string str;
    int pos;
    std::string m;

    strwalker(const std::string& str) : str(str), pos(0) {}

    bool digits(int width = -1) {
        m.clear();
        for (int i = 0; pos+i < m.size(); ++i) {
            char c = str[pos+i];
            if (c < '0' || '9' < c) break;
            m.push_back(c);
            if (i == width-1) break;
        }
        if (m.empty()) { return false; }
        pos += m.size();
        return true;
    }
    bool any(const std::string& pat, int width = -1) {
        m.clear();
        for (int i = 0; pos+i < m.size(); ++i) {
            char c = str[pos+i];
            if (pat.find(c) == std::string::npos) break;
            m.push_back(c);
            if (i == width-1) break;
        }
        if (m.empty()) { return false; }
        pos += m.size();
        return true;
    }
    bool is(char c) {
        m.clear();
        if (str[pos] != c) return false;
        m.push_back(c);
        pos += 1;
        return true;
    }
    bool eos() { return pos == str.size() - 1; }
    int as_int() { return std::stoi(m); }
};

time_t parse_datetime(const std::string& str, int default_tz=0) {
    // "2016-8-10 20:20:35+0900"
    // "2016-8-10 20:20:35+09:00"
    // "2016-8-10 20:20:35Z"
    // "2016-08-10 20:20:35"
    // "2016-08-10 20:20:35"
    // "2016/08/10 20:20:35"
    auto sw = strwalker(str);
    std::tm tm;
    bool date_only = false;
    bool tz_default = true;
    int tz_hour, tz_minute;
    if (!sw.digits(4)) { return -1; }
    tm.tm_year = sw.as_int();
    if (tm.tm_year <= 24 && sw.is(':')) {
        // time only
        tm.tm_hour = tm.tm_year;
        tm.tm_year = 0;
    } else {
        char sep;
        if (!sw.any("/-", 1)) { return -1; }
        sep = sw.m[0];
        if (!sw.digits(2)) { return -1; }
        tm.tm_mon = sw.as_int();
        if (!sw.is(sep)) { return -1; }
        if (!sw.digits(2)) { return -1; }
        tm.tm_mday = sw.as_int();
        if (!sw.any("T ", 1)) {
            date_only = true;
        } else {
            if (!sw.digits(2)) { return -1; }
            tm.tm_hour = sw.as_int();
            if (!sw.is(':')) { return -1; }
        }
    }
    if (!date_only) {
        if (!sw.digits(2)) { return -1; }
        tm.tm_min = sw.as_int();
        if (!sw.is(':')) { return -1; }
        if (!sw.digits(2)) { return -1; }
        tm.tm_sec = sw.as_int();
        if (sw.is('.')) {
            // milli second. skip.
            if (!sw.digits(3)) { return -1; }
        }
        if (!sw.eos()) {
            if (sw.is('Z')) {
                // utc
                tz_minute = tz_hour = 0;
                tz_default = false;
            } else if (sw.any("+-", 1)) {
                bool plus = sw.m[0] == '+';
                if (!sw.digits(2)) { return -1; }
                tz_hour = sw.as_int();
                sw.is(':');
                if (!sw.digits(2)) { return -1; }
                tz_minute = sw.as_int();
                tz_default = false;
            }
        }
    }
    tm.tm_mday -= 1;
    tm.tm_year -= 1900;
    return std::mktime(&tm);
}

inline
std::string isoformat(time_t time, int tz_seconds=0) {
    time += tz_seconds;
    auto tm = std::gmtime(&time);
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(4) << (tm->tm_year + 1900) << '-';
    ss << std::setfill('0') << std::setw(2) << (tm->tm_mon + 1) << '-';
    ss << std::setfill('0') << std::setw(2) << (tm->tm_mday) << 'T';
    ss << std::setfill('0') << std::setw(2) << (tm->tm_hour) << ':';
    ss << std::setfill('0') << std::setw(2) << (tm->tm_min) << ':';
    ss << std::setfill('0') << std::setw(2) << (tm->tm_sec);
    if (tz_seconds == 0) {
        ss << 'Z';
        return ss.str();
    }
    if (tz_seconds < 0) {
        tz_seconds = -tz_seconds;
        ss << '-';
    } else {
        ss << '+';
    }
    int minutes = tz_seconds / 60;
    int hour = minutes / 60;
    int minute = minutes % 60;
    ss << std::setfill('0') << std::setw(2) << (hour);
    ss << std::setfill('0') << std::setw(2) << (minute);
    return ss.str();
}


}
}