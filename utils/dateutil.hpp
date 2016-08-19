#pragma once

#include <stdio.h>
#include <iostream>
#include <algorithm>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <tuple>

namespace xlsxconverter {
namespace utils {
namespace dateutil {

struct strwalker
{
    const std::string& str;
    int pos;
    std::string m;

    strwalker(const std::string& str, size_t pos_=0) : str(str), pos(pos_) {}

    template<class F>
    bool match(const F& f, int width) {
        m.clear();
        for (int i = 0; pos+i < str.size(); ++i) {
            char c = str[pos+i];
            if (!f(c)) break;
            m.push_back(c);
            if (i == width-1) break;
        }
        if (m.empty()) { return false; }
        pos += m.size();
        return true;
    }

    struct digitfn {
        inline bool operator()(char& c) const { return '0' <= c || c <= '9'; }
    };
    inline
    bool digits(int width = -1) {
        return match(digitfn(), width);
    }

    struct anyfn {
        const std::string& pat;
        inline anyfn(const std::string& pat) : pat(pat) {} 
        inline bool operator()(char& c) const { return pat.find(c) != std::string::npos; }
    };
    inline
    bool any(const std::string& pat, int width = -1) {
        return match(anyfn(pat), width);
    }

    struct isfn { 
        const char& pat;
        inline isfn(const char& pat) : pat(pat) {} 
        inline bool operator()(char& c) const { return c == pat; }
    };
    inline
    bool is(char c, int width=1) {
        return match(isfn(c), width);
    }
    inline
    bool is(const std::string& pat) {
        auto sub = str.substr(pos, pos + pat.size());
        if (sub != pat) {
            return false;
        }
        pos += sub.size();
        return true;
    }
    inline bool eos() { return pos >= str.size(); }
    inline int as_int() { return std::stoi(m); }
};

inline
std::tuple<bool, int, int, int, size_t>
parse_date(const std::string& str, size_t pos=0) {
    auto sw = strwalker(str, pos);
    auto failed = std::make_tuple(false, 0, 0, 0, (size_t)0);
    int year = 0;
    int month = 0;
    int day = 0;
    if (!sw.digits(4)) { return failed; }
    year = sw.as_int();
    if (!sw.any("/-", 1)) { return failed; }
    char sep = sw.m[0];
    if (!sw.digits(2)) { return failed; }
    month = sw.as_int();
    if (!sw.is(sep)) { return failed; }
    if (!sw.digits(2)) { return failed; }
    day = sw.as_int();
    if (sw.digits(1)) { return failed; }
    if (month < 1 || 12 < month) { return failed; }
    if (day < 1 || 31 < day) { return failed; }
    return std::make_tuple(true, year, month, day, sw.pos);
}

inline
std::tuple<bool, int, int, int, int, size_t>
parse_time(const std::string& str, size_t pos=0) {
    auto sw = strwalker(str, pos);
    auto failed = std::make_tuple(false, 0, 0, 0, 0, (size_t)0);
    int hour = 0, minute = 0, second = 0, millisecond = 0;
    if (!sw.digits(2)) { return failed; }
    hour = sw.as_int();
    if (!sw.is(':')) { return failed; }
    if (!sw.digits(2)) { return failed; }
    minute = sw.as_int();
    if (sw.is(':')) {
        if (!sw.digits(2)) { return failed; }
        second = sw.as_int();
        if (sw.is('.')) {
            if (!sw.digits(3)) { return failed; }
            millisecond = sw.as_int();
        }
    }
    sw.is(' ');
    if (sw.is("am") || sw.is("AM")) {}
    else if (sw.is("pm") || sw.is("PM")) { hour += 12; }
    if (sw.digits(1)) { return failed; }
    if (hour < 0 || 23 < hour) { return failed; }
    if (minute < 0 || 59 < minute) { return failed; }
    if (second < 0 || 60 < second) { return failed; }
    return std::make_tuple(true, hour, minute, second, millisecond, sw.pos);
}

inline
std::tuple<bool, int, int, size_t>
parse_timezone(const std::string& str, size_t pos=0) {
    auto sw = strwalker(str, pos);
    auto failed = std::make_tuple(false, 0, 0, (size_t)0);
    int tz_hour = 0, tz_minute = 0;
    if (sw.is('Z')) {
        return std::make_tuple(true, 0, 0, sw.pos);
    }
    if (!sw.any("+-")) { return failed; }
    int sign = sw.m[0] == '+' ? 1 : -1;
    if (!sw.digits(2)) { return failed; }
    tz_hour = sw.as_int();
    sw.is(':');
    if (!sw.digits(2)) { return failed; }
    tz_minute = sw.as_int();
    if (sw.digits(1)) { return failed; }
    tz_hour *= sign;
    tz_minute *= sign;
    if (tz_hour < -23 || 23 < tz_hour) { return failed; }
    if (tz_minute < 0 || 59 < tz_minute) { return failed; }
    return std::make_tuple(true, tz_hour, tz_minute, sw.pos);
}

const time_t ntime = -0x80000000;

inline
time_t parse(const std::string& str, int default_tz_seconds=0) {
    auto sw = strwalker(str);
    bool ok1 = false, ok2 = false, ok3 = false;
    int year = 0, month = 0, day = 0;
    int hour = 0, minute = 0, second = 0, millisecond = 0;
    int tz_hour = 0, tz_minute = 0;
    size_t pos;
    std::tie(ok1, year, month, day, sw.pos) = parse_date(str);
    bool date_only = false;
    if (ok1 && sw.eos()) {
        // date-only
        date_only = true;
    }
    else if (ok1) {
        // expect date and time
        if (!sw.any(" T")) { return ntime; }
    }
    if (!date_only) {
        std::tie(ok2, hour, minute, second, millisecond, sw.pos) = parse_time(str, sw.pos);
    }
    if (!ok1 && !ok2) {
        return ntime;
    }
    if (ok2) {
        std::tie(ok3, tz_hour, tz_minute, sw.pos) = parse_timezone(str, sw.pos);
    }
    auto tm = std::tm();
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    if (ok3) {
        default_tz_seconds = (tz_hour * 60 + tz_minute) * 60;
    }
    return std::mktime(&tm) - default_tz_seconds;
}

inline
std::string isoformat(time_t time, int tz_seconds=0, char time_prefix='T', bool hide_tz=false) {
    time += tz_seconds;
    auto tm = std::gmtime(&time);
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(4) << (tm->tm_year + 1900) << '-';
    ss << std::setfill('0') << std::setw(2) << (tm->tm_mon + 1) << '-';
    ss << std::setfill('0') << std::setw(2) << (tm->tm_mday) << time_prefix;
    ss << std::setfill('0') << std::setw(2) << (tm->tm_hour) << ':';
    ss << std::setfill('0') << std::setw(2) << (tm->tm_min) << ':';
    ss << std::setfill('0') << std::setw(2) << (tm->tm_sec);
    if (hide_tz) {
        return ss.str();
    }
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

inline
int local_tz_seconds() {
    time_t local_time;
    std::time(&local_time);
    auto utc_tm = *std::gmtime(&local_time);
    time_t utc_time = std::mktime(&utc_tm);
    return local_time - utc_time;
}

}
}
}
