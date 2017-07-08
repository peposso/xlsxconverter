// Copyright (c) 2016 peposso All Rights Reserved.
// Released under the MIT license
#pragma once
#include <stdio.h>

#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <tuple>


namespace xlsxconverter {
namespace utils {
namespace strutil {

inline bool islower(char c) {
    return 'a' <= c && c <= 'z';
}
inline bool isupper(char c) {
    return 'A' <= c && c <= 'Z';
}

inline std::string lower(const std::string& s) {
    std::string r;
    for (auto c : s) {
        if (islower(c)) {
            r.push_back(c - 'A' + 'a');
        } else {
            r.push_back(c);
        }
    }
    return r;
}
inline std::string upper(const std::string& s) {
    std::string r;
    for (auto c : s) {
        if (islower(c)) {
            r.push_back(c - 'a' + 'A');
        } else {
            r.push_back(c);
        }
    }
    return r;
}
// camelCaseString -> camel_case_string
// Test1TTest_Abc -> test1_ttest_abc
inline std::string snake_case(const std::string& s) {
    std::string r;
    for (auto it = s.begin(); it != s.end(); ++it) {
        if (it != s.begin() && *(it-1) != '_' && !isupper(*(it-1)) && isupper(*it)) {
            r.push_back('_');
        }
        if (isupper(*it)) {
            r.push_back(*it - 'A' + 'a');
        } else {
            r.push_back(*it);
        }
    }
    return r;
}
// snake_case_string -> snakeCaseString
// _thank_you4your__kindness -> ThankYou4YourKindness
inline std::string lower_camel(const std::string& s) {
    std::string r;
    for (auto it = s.begin(); it != s.end(); ++it) {
        if (*it == '_') continue;
        if (it != s.begin() && !std::isalpha(*(it-1)) && islower(*it)) {
            r.push_back(*it - 'a' + 'A');
        } else if (it != s.begin() && std::isalpha(*(it-1)) && isupper(*it)) {
            r.push_back(*it - 'A' + 'a');
        } else {
            r.push_back(*it);
        }
    }
    return r;
}
// snake_case_string -> SnakeCaseString
// _thank_you4your__kindness -> ThankYou4YourKindness
inline std::string upper_camel(const std::string& s) {
    std::string r;
    for (auto it = s.begin(); it != s.end(); ++it) {
        if (*it == '_') continue;
        auto b = *(it-1);
        if (it == s.begin() && islower(*it)) {
            r.push_back(*it - 'a' + 'A');
        } else if (it != s.begin() && !std::isalpha(*(it-1)) && islower(*it)) {
            r.push_back(*it - 'a' + 'A');
        } else if (it != s.begin() && std::isalpha(*(it-1)) && isupper(*it)) {
            r.push_back(*it - 'A' + 'a');
        } else {
            r.push_back(*it);
        }
    }
    return r;
}

}  // strutil
}
}
