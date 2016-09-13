// Copyright (c) 2016 peposso All Rights Reserved.
// Released under the MIT license
#pragma once
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

#include <iterator>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#endif

namespace xlsxconverter {
namespace utils {
namespace fs {

#ifdef _WIN32
inline std::string wtou8(WCHAR* ws) {
    size_t size = ::WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
    std::string buf(size+1, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, ws, -1, &buf[0], size+1, nullptr, nullptr);
    return buf;
}
inline std::vector<WCHAR> u8tow(const std::string& str) {
    size_t size = ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::vector<WCHAR> buf;
    buf.resize(size + 1);
    ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &buf[0], size + 1);
    buf[size] = '\0';
    return buf;
}
#endif

inline char sep() {
    #ifdef _WIN32
    return '\\';
    #else
    return '/';
    #endif
}
inline
std::string joinpath(const std::string& path1, const std::string& path2) {
    if (path1.empty()) return path2;
    if (path2.empty()) return path1;
    char c1 = path1[path1.size()-1];
    if (c1 == '/' || c1 == '\\') {
        return joinpath(path1.substr(0, path1.size()-1), path2);
    }
    char c2 = path2[0];
    if (c2 == '/' || c2 == '\\') {
        return joinpath(path1, path2.substr(1));
    }
    return path1 + sep() + path2;
}
template<class...T>
std::string joinpath(const std::string& path1, const std::string& path2, const T&...t) {
    return joinpath(joinpath(path1, path2), t...);
}
inline
bool exists(const std::string& name) {
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
    if (p1 == std::string::npos) return name.substr(0, p2);
    if (p2 == std::string::npos) return name.substr(0, p1);
    return name.substr(0, p1 > p2 ? p1 : p2);
}
inline
std::string basename(const std::string& name) {
    auto p1 = name.rfind('/');
    auto p2 = name.rfind('\\');
    if (p1 == std::string::npos && p1 == std::string::npos) return name;
    if (p1 == std::string::npos) return name.substr(p2 + 1);
    if (p2 == std::string::npos) return name.substr(p1 + 1);
    return name.substr((p1 > p2 ? p1 : p2) + 1);
}
inline
bool startswith(const std::string& haystack, const std::string& needle) {
    if (haystack.size() < needle.size()) return false;
    if (haystack.size() == needle.size()) return haystack == needle;
    return haystack.substr(0, needle.size()) == needle;
}
inline
bool endswith(const std::string& haystack, const std::string& needle) {
    if (haystack.size() < needle.size()) return false;
    if (haystack.size() == needle.size()) return haystack == needle;
    return haystack.substr(haystack.size() - needle.size()) == needle;
}

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

inline
bool match(const std::string& haystack, const std::string& needle) {
    // simulate windows match pattern. ("*.*" match all.)
    if (needle.empty()) return true;
    if (needle == "*" || needle == "*.*") return true;

    auto pats = split(needle, '*');
    if (pats.size() == 1) {
        // no *
        return haystack == pats[0];
    }
    size_t b = 0;
    size_t e = haystack.size();
    // startswith
    auto pat = pats[0];
    if (!pat.empty()) {  // "aaa*..."
        auto r = startswith(haystack, pat);
        if (!r) return false;
        b += pat.size();
    }
    pats.erase(pats.begin());

    // endswith
    pat = pats[pats.size()-1];
    if (!pat.empty()) {  // "...*aaa"
        auto r = endswith(haystack, pat);
        if (!r) return false;
        e -= pat.size();
    }
    pats.pop_back();

    for (auto pat : pats) {
        auto pos = haystack.find(pat, b);
        if (pos == std::string::npos) return false;
        b = pos + pat.size();
        if (b > e) return false;
    }
    return true;
}

inline
bool mkdirp(const std::string& name) {
    if (name.empty()) return true;
    if (exists(name)) return true;
    if (!mkdirp(dirname(name))) {
        return false;
    }
    #ifdef _WIN32
    auto w = u8tow(name);
    ::CreateDirectoryW(w.data(), nullptr);
    #else
    ::mkdir(name.c_str(), 0755);
    #endif
    return true;
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

struct iterdir {
    struct entry  {
        std::string dirname;
        std::string name;
        bool isfile = false;
        bool isdir = false;
        bool islink = false;
        uint64_t size_ = 0;
    };
    #ifdef _WIN32
    struct iterator : public std::iterator<std::input_iterator_tag, entry> {
        std::string dirname;
        std::string filter;
        HANDLE hfind = INVALID_HANDLE_VALUE;
        WIN32_FIND_DATA find_data;
        entry current;
        int index;
        inline iterator(const std::string& dirname, const std::string& filter, int index = 0)
                : dirname(dirname), filter(filter), current(), index(index),
                  hfind(INVALID_HANDLE_VALUE) {
            current.dirname = dirname;
            if (index < 0) return;
            std::string glob_path;
            if (filter.empty()) {
                glob_path = joinpath(dirname, "*.*");
            } else {
                glob_path = joinpath(dirname, filter);
            }
            hfind = ::FindFirstFile(glob_path.c_str(), &find_data);
        }
        inline ~iterator() {
            if (hfind != INVALID_HANDLE_VALUE) ::FindClose(hfind);
        }
        inline entry& operator*() {
            current.name = find_data.cFileName;
            current.isdir = find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
            current.isfile = !current.isdir;
            current.size_ = (((uint64_t)find_data.nFileSizeHigh << 32) |
                             (uint64_t)find_data.nFileSizeLow);
            return current;
        }
        inline iterator& operator++() {
            if (index < 0 || hfind == INVALID_HANDLE_VALUE) return *this;
            if (!::FindNextFile(hfind, &find_data)) {
                index = -1;
            }
            return *this;
        }
        inline bool operator!=(const iterator& it) {
            return index != it.index || dirname != it.dirname;
        }
        inline bool ends() { return index == -1; }
    };
    #else
    struct iterator : public std::iterator<std::input_iterator_tag, entry> {
        std::string dirname;
        std::string filter;
        ::DIR* dirptr;
        entry current;
        int index;
        inline iterator(const std::string& dirname, const std::string& filter, int index = 0)
                : dirname(dirname), filter(filter),
                  dirptr(nullptr), current(), index(index) {
            current.dirname = dirname;
            operator++();
        }
        inline ~iterator() {
            if (dirptr != nullptr) ::closedir(dirptr);
        }
        inline entry& operator*() {
            return current;
        }
        inline iterator& operator++() {
            if (index < 0) return *this;
            if (dirptr == nullptr) dirptr = ::opendir(dirname.c_str());
            if (dirptr == nullptr) return *this;
            auto entptr = ::readdir(dirptr);
            ++index;
            if (entptr == nullptr) {
                index = -1;
                return *this;
            }
            current.name = entptr->d_name;
            current.isfile = entptr->d_type == DT_REG;
            current.isdir = entptr->d_type == DT_DIR;
            current.islink = entptr->d_type == DT_LNK;
            if (!filter.empty() && !match(current.name, filter)) {
                return operator++();
            }
            return *this;
        }
        inline bool operator!=(const iterator& it) {
            return index != it.index || dirname != it.dirname;
        }
        inline bool ends() { return index == -1; }
    };
    #endif

    std::string dirname;
    std::string filter;
    inline explicit iterdir(const std::string& dirname, const std::string& filter = "")
        : dirname(dirname), filter(filter) {}
    inline iterator begin() { return iterator(dirname, filter); }
    inline iterator end() { return iterator(dirname, filter, -1); }
};

std::vector<std::string> walk(const std::string& dirname, const std::string& filter = "") {
    std::vector<std::string> files;
    for (auto& entry : iterdir(dirname)) {
        if (entry.isdir) {
            if (entry.name == ".") continue;
            if (entry.name == "..") continue;
            auto dir = joinpath(dirname, entry.name);
            for (auto& child : walk(dir, filter)) {
                files.push_back(joinpath(entry.name, child));
            }
        } else if (entry.isfile && match(entry.name, filter)) {
            files.push_back(entry.name);
        }
    }
    return files;
}

}  // namespace fs
}  // namespace utils
}  // namespace xlsxconverter
