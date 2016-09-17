#include "utils.hpp"

int main(int argc, char** argv) {
    using namespace xlsxconverter;
    using namespace xlsxconverter::utils::dateutil;
    bool ok;
    size_t p;
    int y, m, d, h, i, s, ms, zh, zm;
    std::tie(ok, y, m, d, p) = parse_date("1970-01-01");
    std::tie(ok, h, i, s, ms, p) = parse_time("1:02:3");
    utils::log("y=", y, " m=", m, " d=", d);
    utils::log("h=", h, " i=", i, " s=", s);

    std::tie(ok, y, m, d, h, i, s, ms, zh, zm, p) = parse_tuple("1970-01-01T00:00:00");
    utils::log("y=", y, " m=", m, " d=", d);
    utils::log("h=", h, " i=", i, " s=", s);

    // 25569
    utils::log("days=", make_days(1, 1, 1));
    utils::log("days=", make_days(1970, 1, 1));
    utils::log("days=", make_days(1970, 1, 1) - make_days(1899, 12, 30));
    std::tie(y, m, d) = make_date_tuple(25569);
    utils::log("y=", y, " m=", m, " d=", d);

    utils::log("time=", parse64("1970-01-01T00:00:00"));
    if (parse64("1970-01-01T00:00:00") != 0) {
        throw utils::exception("epoch");
    }

    utils::log("sizeof(time_t)=", sizeof(time_t));
    for (int i = 0; i < 86400; ++i) {
        time_t t = -i;
        auto tm = std::gmtime(&t);
        if (tm == nullptr) {
            utils::log("tm=null");
            break;
        } else {
            utils::log("tm=", tm->tm_year+1900, "-", tm->tm_mon+1, "-", tm->tm_mday, "T", tm->tm_hour, ":", tm->tm_min, ":", tm->tm_sec);
        }
    }
    

    return 0;
}
