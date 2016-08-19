#pragma once
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <algorithm>
#include <iterator>
#include <atomic>
#include <list>
#include <unordered_map>
#include <clocale>
#include <fstream>

#include <boost/optional.hpp>

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
    std::istringstream ss(str);
    std::string item;
    std::vector<std::string> result;
    while (std::getline(ss, item, delim)) {
        result.push_back(item);
    }
    if (result.empty()) result.resize(1);
    return result;
}
template<class...A>
std::vector<std::string> split(const std::string& str, char delim, A...a) {
    std::vector<std::string> result;
    for (auto& item: split(str, delim)) {
        for (auto& child: split(item, a...)) {
            result.push_back(child);
        }
    }
    return result;
}
inline
bool fexists(const std::string& name) {
    struct stat statbuf;
    return ::stat(name.c_str(), &statbuf) == 0;
}
inline
bool isabspath(const std::string& name) {
    if (name.empty()) return false;
    char c = name[0];
    if (c == '/') return true;
    if (name.size() == 1) return false;
    if (std::isalpha(c) && name[1] == ':') {
        if (name.size() == 2) return true;
        if (name[2] == '/' || name[2] == '\\') return true;
    }
    if (c == '\\' && name[1] == '\\') return true;
    return false;
}
inline
std::string dirname(const std::string& name) {
    auto p1 = name.rfind('/');
    auto p2 = name.rfind('\\');
    if (p1 == std::string::npos && p1 == std::string::npos) return "";
    if (p1 != std::string::npos) return name.substr(0, p1);
    return name.substr(0, p2);
}

inline
void mkdirp(const std::string& name) {
    if (name.empty()) return;
    auto names = split(name, '/', '\\');
    #ifdef WIN32
    auto sep = '\\';
    #else
    auto sep = '/';
    #endif
    std::string path;
    if (isabspath(name)) {
        path = names[0];
        names.erase(names.begin());
    }
    for (auto n: names) {
        path = path.empty() ? n : path + sep + n;
        if (!fexists(path)) {
            #if _WIN32
            ::_mkdir(path.c_str());
            #else
            ::mkdir(path.c_str(), 0755);
            #endif
        } 
    }
}
inline
std::string readfile(const std::string& name) {
    auto fi = std::ifstream(name.c_str(), std::ios::binary);
    std::stringstream ss;
    ss << fi.rdbuf();
    return ss.str();
}
inline
void writefile(const std::string& name, const std::string& content) {
    mkdirp(dirname(name));
    auto fo = std::ofstream(name.c_str(), std::ios::binary);
    fo << content;
}

template<class T>
bool contains(const std::vector<T> vec, const T& value) {
    return std::find(vec.begin(), vec.end(), value) != vec.end();
}

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

struct exception : public std::runtime_error {
    template<class...A>
    exception(A...a) : std::runtime_error(sscat(a...).c_str()) {}
};

struct spinlock
{
  std::atomic_flag state_;
  inline spinlock() : state_ ATOMIC_FLAG_INIT {}
  inline void lock() {
    while (state_.test_and_set(std::memory_order_acquire));
  }
  inline void unlock() {
    state_.clear(std::memory_order_release);
  }
};

inline spinlock& logging_lock() {
    static spinlock logging_lock;
    return logging_lock;
}

template<class...A>
void log(const A&...a) {
    auto s = sscat(a..., '\n');
    std::lock_guard<spinlock> lock(logging_lock());
    std::cout << s;
}

template<class...A>
void logerr(const A&...a) {
    auto s = sscat(a..., '\n');
    std::lock_guard<spinlock> lock(logging_lock());
    std::cerr << s;
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


inline
std::tuple<bool, uint16_t, uint16_t> u32to16char(uint32_t c0) {
    if ((c0 & ~0xFFFF) == 0) {
        return std::make_tuple(false, c0, 0);
    }
    // sarrogate pair
    uint16_t c1 = 0xD800 | ((((c0 & 0x1F0000) - 0x010000) | (c0 & 0x00FC00)) >> 10);
    uint16_t c2 = 0xDC00 | (c0 & 0x0003FF);
    return std::make_tuple(true, c1, c2);
}


template<class T, class M=std::mutex>
struct mutex_list
{
    struct not_found : public std::exception {};
    M mutex;
    std::list<T> list;

    inline mutex_list()
        : mutex(),
          list()
    {}
    inline void push_back(T t) {
        std::lock_guard<M> lock(mutex);
        list.push_back(std::move(t));
    }
    inline boost::optional<T> move_front(bool* last_ptr=nullptr) {
        std::lock_guard<M> lock(mutex);
        if (list.empty()) return boost::none;
        T t = std::move(list.front());
        list.pop_front();
        if (last_ptr != nullptr) *last_ptr = list.empty();
        return t;
    }
    inline boost::optional<T> move_back(bool* last_ptr=nullptr) {
        std::lock_guard<M> lock(mutex);
        if (list.empty()) return boost::none;
        T t = std::move(list.back());
        list.pop_back();
        if (last_ptr != nullptr) *last_ptr = list.empty();
        return t;
    }
    inline bool empty() {
        std::lock_guard<M> lock(mutex);
        return list.empty();
    }
    template<class F> bool any(F f) {
        std::lock_guard<M> lock(mutex);
        for (auto& e: list) {
            if (f(e)) return true;
        }
        return false;
    }
    template<class F> T& get(F f) {
        std::lock_guard<M> lock(mutex);
        for (auto& e: list) {
            if (f(e)) return e;
        }
        throw not_found();
    }
};


template<class K, class V, class M=std::mutex>
struct mutex_map
{
    M mutex;
    std::unordered_map<K, V> map;

    inline mutex_map()
        : mutex(),
          map()
    {}
    inline void emplace(K k, V v) {
        std::lock_guard<M> lock(mutex);
        map.emplace(std::move(k), std::move(v));
    }
    inline V& emplace_ref(K k, V v) {
        std::lock_guard<M> lock(mutex);
        map.emplace(k, std::move(v));
        auto it = map.find(k);
        return it->second;
    }
    inline boost::optional<V> get(const K& k) {
        std::lock_guard<M> lock(mutex);
        auto it = map.find(k);
        if (it == map.end()) return boost::none;
        return it->second;
    }
    inline bool has(const K& k) {
        std::lock_guard<M> lock(mutex);
        auto it = map.find(k);
        return it != map.end();
    }
    inline bool empty() {
        std::lock_guard<M> lock(mutex);
        return map.empty();
    }
};


}
}

