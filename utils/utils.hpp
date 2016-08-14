#pragma once

#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <algorithm>
#include <iterator>

// macros
#define XLSXCONVERTER_UTILS_DISABLE_ANY(...) \
    xlsxconverter::utils::enable_if_t< \
        !xlsxconverter::utils::anytypeof<__VA_ARGS__>::value> = nullptr
#define XLSXCONVERTER_UTILS_ENABLE_ANY(...) \
    xlsxconverter::utils::enable_if_t< \
        xlsxconverter::utils::anytypeof<__VA_ARGS__>::value> = nullptr
#define XLSXCONVERTER_UTILS_EXCEPTION(...) \
    xlsxconverter::utils::exception( \
        __VA_ARGS__, "\n    at ", __FILE__, ':', __LINE__, " in ", __func__)

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

template<class...A>
void nop(A...) {}

template<class...A>
std::string sscat(const A&...a) {
    auto ss = std::stringstream();
    nop((ss << a, 0) ...);
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

        inline uint32_t operator*() {
            if (index >= str.size()) {
                index = str.size();
                return 0;
            }
            uint32_t c0 = str[index];
            switch (width_(c0)) {
                case 1: {
                    return c0;
                }
                case 2: {
                    uint32_t c1 = check_(str[index+1]);
                    return ((c0 & 0x1F) << 6) | (c1 & 0x3F);
                }
                case 3: {
                    uint32_t c1 = check_(str[index+1]);
                    uint32_t c2 = check_(str[index+2]);
                    return ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
                }
                case 4: {
                    uint32_t c1 = check_(str[index+1]);
                    uint32_t c2 = check_(str[index+2]);
                    uint32_t c3 = check_(str[index+3]);
                    // upper 5bit
                    c0 = ((c0 & 0x7) << 2) | ((c1 >> 4) & 0x3);
                    if (c0 > 0x10) { throw exception("invalid utf8"); }
                    return (c0 << 16) | ((c1 & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                }
            }
            throw exception("");
        }
        inline iterator& operator++() {
            if (index >= str.size()) {
                index = str.size();
                return *this;
            }
            index += width_(str[index]);
            if (index > str.size()) { index = str.size(); }
            return *this;
        }
        inline bool operator!=(const iterator& it) {
            return index != it.index || str != it.str;
        }
        inline uint8_t check_(uint8_t c) {
            if ((c & 0xc0) != 0x80) { throw 0; }
            return c;
        }
        inline int width_(uint8_t c) {
            if ((c & 0x80) == 0) {
                // U+0000 - U+007F
                return 1;
            } else if ((c & 0xE0) == 0xC0) {
                // U+0080 - U+07FF
                return 2;
            } else if ((c & 0xF0) == 0xE0) {
                // U+0800 - U+FFFF
                return 3;
            } else if (0xF0 <= c && c <= 0xF4) {
                // U+10000 - U+10FFFF
                return 4;
            } else {
                throw exception("invalid utf8 charactor c=0x", std::hex, (int)c, std::dec);
            }
        }
    };

    std::string str;

    inline u8to32iter(const std::string& str) : str(str) {}
    inline iterator begin() { return iterator(str, 0); }
    inline iterator end() { return iterator(str, str.size()); }
};

}
}

