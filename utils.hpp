#pragma once

#include <stdio.h>
#include <iostream>
#include <sys/stat.h>
#include <algorithm>
#include <iterator>

#include "utils/dateutil.hpp"

namespace xlsxconverter {
namespace utils {

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

}
}

