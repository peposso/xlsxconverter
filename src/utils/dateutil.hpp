// Copyright (c) 2016 peposso All Rights Reserved.
// Released under the MIT license
#pragma once
#include <stdio.h>
#include <ctime>

#include <iostream>
#include <algorithm>
#include <vector>
#include <sstream>
#include <iomanip>
#include <tuple>

namespace xlsxconverter {
namespace utils {
namespace dateutil {

// on mingw, gmtime does not support time before epoch - 12h. (1969-12-31T12:0:0)

// SEE: http://alcor.concordia.ca/~gpkatch/gdate-algorithm.html
inline int64_t make_days(int year, int month, int day) {
    // 0001-1-1 based days.
    int m = (month + 9) % 12;  // mar=0, feb=11 
    int y = year - m/10;  // if Jan/Feb, --year
    return y*365 + y / 4 - y / 100 + y / 400 + (m * 306 + 5) / 10 + (day - 1) - 306;
}
inline
std::tuple<int, int, int> make_date_tuple(int64_t days) {
    // 0001-1-1 based days.
    days += 306;
    int y = (10000*days + 14780)/3652425;
    int ddd = days - (y*365 + y/4 - y/100 + y/400);
    if (ddd < 0) {
        --y;
        ddd = days - (y*365 + y/4 - y/100 + y/400);
    }
    int mi = (52 + 100*ddd)/3060;
    int dd = ddd - (mi*306 + 5)/10 + 1;
    return std::make_tuple(
        y + (mi + 2)/12,
        (mi + 2)%12 + 1,
        dd);
}
inline
std::tuple<int, int, int> make_time_tuple(int seconds) {
    if (seconds < 0 || 86400 <= seconds) {
        throw std::runtime_error("bad seconds");
    }
    int minutes = seconds / 60;
    return std::make_tuple(minutes / 60, minutes % 60, seconds % 60);
}
inline
std::tuple<int, int, int, int, int, int> make_tuple64(int64_t time) {
    int year, month, day, hour, minute, second;
    std::tie(year, month, day) = make_date_tuple(time / 86400 + 719162);
    std::tie(hour, minute, second) = make_time_tuple(time % 86400);
    return std::make_tuple(year, month, day, hour, minute, second);
}
inline
std::tuple<int, int, int, int, int, int> make_tuple(time_t time_from_epoch) {
    return make_tuple64(time_from_epoch);
}
inline
int64_t make_time64(int year, int month, int day, int hour, int minute, int second) {
    int64_t days = make_days(year, month, day);
    int seconds = hour * 3600 + minute * 60 + second;
    time_t time = (days - 719162) * 86400 + seconds;
    return time;
}
inline
time_t make_time(int year, int month, int day, int hour, int minute, int second) {
    return make_time64(year, month, day, hour, minute, second);
}


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
        inline bool operator()(char& c) const { return '0' <= c && c <= '9'; }
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

const int ntz_hour = 0xFFFFFFFF;

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

inline
std::tuple<bool, int, int, int, int, int, int, int, int, int, size_t>
parse_tuple(const std::string& str) {
    auto sw = strwalker(str);
    bool ok_date = false, ok_time = false, ok_tz = false;
    int year = 0, month = 0, day = 0;
    int hour = 0, minute = 0, second = 0, millisecond = 0;
    int tz_hour = ntz_hour, tz_minute = 0;
    size_t pos;
    auto failed = std::make_tuple(false, 0, 0, 0, 0, 0, 0, 0, ntz_hour, 0, 0);
    std::tie(ok_date, year, month, day, sw.pos) = parse_date(str);
    bool date_only = false;
    if (ok_date && sw.eos()) {
        // date-only
        date_only = true;
    }
    else if (ok_date) {
        // expect date and time
        if (!sw.any(" T")) { return failed; }
    }
    if (!date_only) {
        std::tie(ok_time, hour, minute, second, millisecond, sw.pos) = parse_time(str, sw.pos);
    }
    if (!ok_date && !ok_time) { return failed; }
    if (ok_time) {
        std::tie(ok_tz, tz_hour, tz_minute, sw.pos) = parse_timezone(str, sw.pos);
        if (!ok_tz) tz_hour = ntz_hour;
    }
    return std::make_tuple(true, year, month, day, hour, minute, second, millisecond, tz_hour, tz_minute, sw.pos);
}


const time_t ntime = -0x80000000;

inline
int64_t parse64(const std::string& str, int default_tz_seconds=0) {
    bool ok;
    int year, month, day, hour, minute, second, millisecond, tz_hour, tz_minute;
    size_t pos;
    std::tie(ok, year, month, day, hour, minute, second, millisecond, tz_hour, tz_minute, pos) = parse_tuple(str);
    if (!ok) {
        return ntime;
    }
    if (tz_hour != ntz_hour) {
        default_tz_seconds = (tz_hour * 60 + tz_minute) * 60;
    }
    return make_time64(year, month, day, hour, minute, second) - default_tz_seconds;
}
inline
time_t parse(const std::string& str, int default_tz_seconds=0) {
    return parse64(str, default_tz_seconds);
}

inline
std::string isoformat64(int64_t time, int tz_seconds=0, char time_prefix='T', bool hide_tz=false) {
    time += tz_seconds;
    int year, month, day, hour, minute, second;
    std::tie(year, month, day, hour, minute, second) = make_tuple(time);
    // auto tm = gmtime_alt_(&time);
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(4) << year << '-';
    ss << std::setfill('0') << std::setw(2) << month << '-';
    ss << std::setfill('0') << std::setw(2) << day << time_prefix;
    ss << std::setfill('0') << std::setw(2) << hour << ':';
    ss << std::setfill('0') << std::setw(2) << minute << ':';
    ss << std::setfill('0') << std::setw(2) << second;
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
    int tz_minutes = tz_seconds / 60;
    int tz_hour = tz_minutes / 60;
    int tz_minute = tz_minutes % 60;
    ss << std::setfill('0') << std::setw(2) << tz_hour;
    ss << std::setfill('0') << std::setw(2) << tz_minute;
    return ss.str();
}
inline
std::string isoformat(time_t time, int tz_seconds=0, char time_prefix='T', bool hide_tz=false) {
    return isoformat64(time, tz_seconds, time_prefix, hide_tz);
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
