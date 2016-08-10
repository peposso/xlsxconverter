#pragma once

#include <stdio.h>
#include <iostream>
#include <sys/stat.h>
#include <algorithm>

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


}
}