#pragma once
#include "yaml_config.hpp"
#include "utils.hpp"

#define EXCEPTION XLSXCONVERTER_UTILS_EXCEPTION

namespace xlsxconverter {

struct Validator {
    YamlConfig::Field& field;
    YamlConfig::Field::Validate& validate;
    std::unordered_set<std::string> unique_strset;
    std::unordered_set<int64_t> unique_intset;
    boost::optional<int64_t> prev_intvalue;
    boost::optional<std::string> prev_strvalue;

    inline Validator(YamlConfig::Field& field)
        : field(field), validate(field.validate.value()) {}

    inline void operator()(std::string& val) {
        if (validate.unique) {
            if (unique_strset.count(val) != 0) throw EXCEPTION("unique validation error. value=", val);
            unique_strset.insert(val);
        }
        if (validate.anyof) {
            if (validate.anyof_strset.count(val) == 0) throw EXCEPTION("anyof validation error. value=", val);
        }
        if (validate.min != boost::none) {
            throw EXCEPTION("min validation requires int type. value=", val);
        }
        if (validate.max != boost::none) {
            throw EXCEPTION("max validation requires int type. value=", val);
        }
        if (validate.sorted) {
            if (prev_strvalue != boost::none) {
                if (prev_strvalue.value() > val)
                    throw EXCEPTION("sorted value validation error. value=", val);
            }
        }
        if (validate.sequential) {
            throw EXCEPTION("sequential validation requires int type. value=", val);
        }
        if (validate.sorted || validate.sequential) {
            prev_strvalue = val;
        }
    }
    inline void operator()(int64_t& val) {
        if (validate.unique) {
            if (unique_intset.count(val) != 0) throw EXCEPTION("unique validation error. value=", val);
            unique_intset.insert(val);
        }
        if (validate.anyof) {
            if (validate.anyof_intset.count(val) == 0) throw EXCEPTION("anyof validation error. value=", val);
        }
        if (validate.min != boost::none) {
            if (val < validate.min.value()) throw EXCEPTION("min=", validate.min.value(), " validation error. value=", val);
        }
        if (validate.max != boost::none) {
            if (validate.max.value() < val) throw EXCEPTION("max=", validate.max.value(), " validation error. value=", val);
        }
        if (validate.sorted) {
            if (prev_intvalue != boost::none) {
                if (prev_intvalue.value() > val)
                    throw EXCEPTION("sorted value validation error. value=", val);
            }
        }
        if (validate.sequential) {
            if (prev_intvalue != boost::none) {
                if (prev_intvalue.value() != val && prev_intvalue.value()+1 != val)
                    throw EXCEPTION("sequential value validation error. value=", val);
            }
        }
        if (validate.sorted || validate.sequential) {
            prev_intvalue = val;
        }
    }
};


}

#undef EXCEPTION
