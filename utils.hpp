#pragma once

#include <stdio.h>
#include <iostream>
#include <sys/stat.h>
#include <algorithm>
#include <iterator>

#include "utils/dateutil.hpp"

namespace xlsxconverter {
namespace utils {

template<bool B, class T, class F>
using conditional_t = typename std::conditional<B,T,F>::type;

template<bool B>
using enable_if_t = typename std::enable_if<B, std::nullptr_t>::type;

template<size_t I, class T>
using tuple_element_t = typename std::tuple_element<I, T>::type;

template<class T, class Tuple, size_t I>
struct anytypeof_ {
    static const bool value = std::is_same<T, tuple_element_t<I, Tuple>>::value ? true : anytypeof_<T, Tuple, I-1>::value;
};
template<class T, class Tuple>
struct anytypeof_<T, Tuple, 0> {
    static const bool value = std::is_same<T, tuple_element_t<0, Tuple>>::value;
};
template<class T, class...A>
struct anytypeof : anytypeof_<T, std::tuple<A...> , sizeof...(A)-1> {};


#define DISABLE_ANY(...) xlsxconverter::utils::enable_if_t<!xlsxconverter::utils::anytypeof<__VA_ARGS__>::value> = nullptr
#define ENABLE_ANY(...) xlsxconverter::utils::enable_if_t<xlsxconverter::utils::anytypeof<__VA_ARGS__>::value> = nullptr


inline
std::vector<std::string> split(const std::string& str, char delim) {
    auto ss = std::istringstream(str);

    std::string item;
    std::vector<std::string> result;
    while (std::getline(ss, item, delim)) {
        result.push_back(item);
    }
    return result;
}
inline
bool fexists(const std::string& name) {
    struct stat statbuf;
    return ::stat(name.c_str(), &statbuf) == 0;
}

template<class T>
bool contains(const std::vector<T> vec, const T& value) {
    return std::find(vec.begin(), vec.end(), value) != vec.end();
}

template<class T, class...A>
void sscat_detail_(std::stringstream& ss, const T& t, const A&...a) {
    ss << t;
    sscat_detail_(a...);
}

template<class T>
void sscat_detail_(std::stringstream& ss, const T& t) {
    ss << t;
}

template<class...A>
std::string sscat(const A&...a) {
    auto ss = std::stringstream();
    (void)(int[]){0, ((void)(ss << a), 0)... };
    return ss.str();
}

struct exception : public std::runtime_error
{
    template<class...A>
    exception(A...a) : std::runtime_error(sscat(a...).c_str()) {}
};

template<class...A>
void log(const A&...a) {
    std::cout << sscat(a...) << std::endl;
}

template<class...A>
void logerr(A...a) {
    std::cerr << sscat(a...) << std::endl;
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

