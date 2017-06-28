#include <cmath>
#include <string>

namespace xlsxconverter {
namespace utils {

std::string dtos(double n) {
    static double PRECISION = 0.0000000000001;
    // handle special cases
    if (std::isnan(n)) {
        return "nan";
    } else if (std::isinf(n)) {
        return "inf";
    } else if (n == 0.0) {
        return "0";
    }
    int digit, m, m1;
    std::string s;
    bool neg = (n < 0);
    if (neg) {
        n = -n;
    }
    // calculate magnitude
    m = std::log10(n);
    int useExp = (m >= 14 || (neg && m >= 9) || m <= -9);
    if (neg) {
        s.push_back('-');
    }
    // set up for scientific notation
    if (useExp) {
        if (m < 0)
            m -= 1.0;
        n = n / std::pow(10.0, m);
        m1 = m;
        m = 0;
    }
    if (m < 1.0) {
        m = 0;
    }
    if (n > PRECISION) {
        n += PRECISION * 0.5;
    }
    // convert the number
    while (n > PRECISION || m >= 0) {
        double weight = std::pow(10.0, m);
        if (weight > 0 && !std::isinf(weight)) {
            digit = std::floor(n / weight);
            n -= (digit * weight);
            s.push_back('0' + digit);
        }
        if (m == 0 && n > PRECISION)
            s.push_back('.');
        m--;
    }
    if (useExp) {
        // convert the exponent
        int i, j;
        s.push_back('e');
        if (m1 > 0) {
            s.push_back('+');
        } else {
            s.push_back('-');
            m1 = -m1;
        }
        std::string e;
        m = 0;
        while (m1 > 0) {
            e.push_back('0' + m1 % 10);
            m1 /= 10;
            m++;
        }
        for (int i = 0; i < e.size(); i++) {
            s.push_back(e[e.size() - 1 - i]);
        }
    }
    return s;
}

}  // utils
}  // xlsxconverter
