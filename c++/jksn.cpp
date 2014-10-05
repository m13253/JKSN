/*
  Copyright (c) 2014 StarBrilliant <m13253@hotmail.com>
  All rights reserved.

  Redistribution and use in source and binary forms are permitted
  provided that the above copyright notice and this paragraph are
  duplicated in all such forms and that any documentation,
  advertising materials, and other materials related to such
  distribution and use acknowledge that the software was developed by
  StarBrilliant.
  The name of StarBrilliant may not be used to endorse or promote
  products derived from this software without specific prior written
  permission.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
  IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
*/

#include "jksn.hpp"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <sstream>

using namespace JKSN;

JKSNValue::JKSNValue(const JKSNValue& that): value_type{JKSN_UNDEFINED} {
    switch (that.value_type) {
    case JKSN_BOOL:
        value_bool = that.value_bool;
        break;
    case JKSN_INT:
        value_int = that.value_int;
        break;
    case JKSN_FLOAT:
        value_float = that.value_float;
        break;
    case JKSN_DOUBLE:
        value_double = that.value_double;
        break;
    case JKSN_LONG_DOUBLE:
        value_long_double = that.value_long_double;
        break;
    case JKSN_STRING:
    case JKSN_BLOB:
        new(&value_pstr) auto(that.value_pstr);
        break;
    case JKSN_ARRAY:
        new(&value_parray) auto(that.value_parray);
        break;
    case JKSN_OBJECT:
        new(&value_pobject) auto(that.value_pobject);
        break;
    default:
        break;
    }
    value_type = that.value_type;
}

JKSNValue::JKSNValue(JKSNValue&& that): value_type{JKSN_UNDEFINED} {
    switch (that.value_type) {
    case JKSN_BOOL:
        value_bool = that.value_bool;
        break;
    case JKSN_INT:
        value_int = that.value_int;
        break;
    case JKSN_FLOAT:
        value_float = that.value_float;
        break;
    case JKSN_DOUBLE:
        value_double = that.value_double;
        break;
    case JKSN_LONG_DOUBLE:
        value_long_double = that.value_long_double;
        break;
    case JKSN_STRING:
    case JKSN_BLOB:
        new(&value_pstr) auto(std::move(that.value_pstr));
        break;
    case JKSN_ARRAY:
        new(&value_parray) auto(std::move(that.value_parray));
        break;
    case JKSN_OBJECT:
        new(&value_pobject) auto(std::move(that.value_pobject));
        break;
    default:
        break;
    }
    value_type = that.value_type;
}

JKSNValue& JKSNValue::operator = (const JKSNValue& rhs) {
    if (this == &rhs) {
        return *this;
    }
    else {
        this->~JKSNValue();
        new(this) JKSNValue(rhs);
        return *this;
    }
}

JKSNValue& JKSNValue::operator = (JKSNValue&& rhs) {
    if (this == &rhs) {
        return *this;
    }
    else {
        this->~JKSNValue();
        new(this) JKSNValue(std::move(rhs));
        return *this;
    }
}

JKSNValue::~JKSNValue() {
    switch (value_type) {
    case JKSN_STRING:
    case JKSN_BLOB:
        value_pstr.~shared_ptr();
        break;
    case JKSN_ARRAY:
        value_parray.~shared_ptr();
        break;
    case JKSN_OBJECT:
        value_pobject.~shared_ptr();
        break;
    default:
        break;
    }
}

bool JKSNValue::toBool() const {
    switch (value_type) {
    case JKSN_UNDEFINED:
    case JKSN_NULL:
    case JKSN_UNSPECIFIED:
        return false;
    case JKSN_BOOL:
        return value_bool;
    case JKSN_INT:
        return value_int != 0;
    case JKSN_FLOAT:
        return value_float != 0.0f;
    case JKSN_DOUBLE:
        return value_double != 0.0;
    case JKSN_LONG_DOUBLE:
        return value_long_double != 0.0L;
    case JKSN_STRING:
    case JKSN_BLOB:
        return *value_pstr != "";
    default:
        return true;
    }
}

int64_t JKSNValue::toInt() const {
    switch (value_type) {
    case JKSN_UNDEFINED:
    case JKSN_NULL:
    case JKSN_UNSPECIFIED:
        return 0;
    case JKSN_BOOL:
        return value_bool ? 1 : 0;
    case JKSN_INT:
        return value_int;
    case JKSN_FLOAT:
        return static_cast<int64_t>(value_float);
    case JKSN_DOUBLE:
        return static_cast<int64_t>(value_double);
    case JKSN_LONG_DOUBLE:
        return static_cast<int64_t>(value_long_double);
    case JKSN_STRING:
        return static_cast<int64_t>(std::atoll(value_pstr->c_str()));
    default:
        throw JKSNTypeError{"Cannot convert value to int64_t."};
    }
}

float JKSNValue::toFloat() const {
    switch (value_type) {
    case JKSN_BOOL:
        return value_bool ? 1.0f : 0.0f;
    case JKSN_INT:
        return static_cast<float>(value_int);
    case JKSN_FLOAT:
        return value_float;
    case JKSN_DOUBLE:
        return static_cast<float>(value_double);
    case JKSN_LONG_DOUBLE:
        return static_cast<float>(value_long_double);
    case JKSN_STRING: {
        const char *start = value_pstr->c_str();
        char *end = nullptr;
        float re = std::strtof(start, &end);
        if (re == HUGE_VALF || end != start + value_pstr->size())
            return NAN;
        else
            return re;
    }
    default:
        return NAN;
    }
}

double JKSNValue::toDouble() const {
    switch (value_type) {
    case JKSN_BOOL:
        return value_bool ? 1.0 : 0.0;
    case JKSN_INT:
        return static_cast<double>(value_int);
    case JKSN_FLOAT:
        return static_cast<double>(value_float);
    case JKSN_DOUBLE:
        return value_double;
    case JKSN_LONG_DOUBLE:
        return static_cast<double>(value_long_double);
    case JKSN_STRING: {
        const char *start = value_pstr->c_str();
        char *end = nullptr;
        double re = std::strtod(start, &end);
        if (re == HUGE_VAL || end != start + value_pstr->size())
            return NAN;
        else
            return re;
    }
    default:
        return NAN;
    }
}

long double JKSNValue::toLongDouble() const {
    switch (value_type) {
    case JKSN_BOOL:
        return value_bool ? 1.0L : 0.0L;
    case JKSN_INT:
        return static_cast<long double>(value_int);
    case JKSN_FLOAT:
        return static_cast<long double>(value_float);
    case JKSN_DOUBLE:
        return static_cast<long double>(value_double);
    case JKSN_LONG_DOUBLE:
        return value_long_double;
    case JKSN_STRING: {
        const char *start = value_pstr->c_str();
        char *end = nullptr;
        long double re = std::strtold(start, &end);
        if (re == HUGE_VALL || end != start + value_pstr->size())
            return NAN;
        else
            return re;
    }
    default:
        return NAN;
    }
}

std::string JKSNValue::toString() const {
    std::ostringstream os;
    switch (value_type) {
    case JKSN_UNDEFINED:
        return "undefined";
    case JKSN_NULL:
        return "null";
    case JKSN_BOOL:
        return value_bool ? "true" : "false";
    case JKSN_INT:
        os << value_int;
        return os.str();
    case JKSN_FLOAT:
        os << value_float;
        return os.str();
    case JKSN_DOUBLE:
        os << value_double;
        return os.str();
    case JKSN_LONG_DOUBLE:
        os << value_long_double;
        return os.str();
    case JKSN_STRING:
    case JKSN_BLOB:
        return *value_pstr;
    case JKSN_ARRAY: {
        os << '[';
        bool comma = false;
        for (auto& i : *value_parray) {
            if (comma)
                os << ", ";
            comma = true;
            os << i.toString();
        }
        os << ']';
        return os.str();
    }
    case JKSN_OBJECT: {
        os << '{';
        bool comma = false;
        for (auto& i : *value_pobject) {
            if (comma)
                os << ", ";
            comma = true;
            os << i.first.toString() << " : " << i.second.toString();
        }
        os << '}';
        return os.str();
    }
    case JKSN_UNSPECIFIED:
        throw JKSNTypeError{"toString() on unspecified value."};
    }
}

std::string JKSNValue::toBlob() const {
    if (value_type == JKSN_BLOB)
        return *value_pstr;
    else
        throw JKSNTypeError{"Cannot convert value to blob."};
}

JKSNValue::Array JKSNValue::toArray() const {
    if (value_type == JKSN_ARRAY)
        return *value_parray;
    else
        return {*this};
}

JKSNValue::Object JKSNValue::toObject() const {
    switch (value_type) {
    case JKSN_ARRAY: {
        Object res;
        for (size_t i = 0; i < value_parray->size(); i++) {
            res[static_cast<int64_t>(i)] = (*value_parray)[i];
        }
        return res;
    }
    case JKSN_OBJECT:
        return *value_pobject;
    default:
        throw JKSNTypeError{"Cannot convert value to object."};
    }
}

bool JKSNValue::operator == (const JKSNValue& rhs) const {
    if (value_type != rhs.value_type) {
        return false;
    }
    else {
        switch (value_type) {
        case JKSN_BOOL:
        case JKSN_INT:
        case JKSN_FLOAT:
        case JKSN_DOUBLE:
        case JKSN_LONG_DOUBLE:
            return toLongDouble() == rhs.toLongDouble();
        case JKSN_STRING:
        case JKSN_BLOB:
            return value_pstr == rhs.value_pstr || *value_pstr == *rhs.value_pstr;
        case JKSN_ARRAY:
            return value_parray == rhs.value_parray || *value_parray == *rhs.value_parray;
        case JKSN_OBJECT:
            return value_pobject == rhs.value_pobject || *value_pobject == *rhs.value_pobject;
        default:
            return true;
        }
    }
}

size_t JKSNValue::hashCode() const {
    switch (value_type) {
    case JKSN_BOOL:
        return std::hash<bool>{}(value_bool);
    case JKSN_INT:
        return std::hash<int64_t>{}(value_int);
    case JKSN_FLOAT:
        return std::hash<float>{}(value_float);
    case JKSN_DOUBLE:
        return std::hash<double>{}(value_double);
    case JKSN_LONG_DOUBLE:
        return std::hash<long double>{}(value_long_double);
    case JKSN_STRING:
    case JKSN_BLOB:
        return std::hash<std::string>{}(*value_pstr);
    case JKSN_ARRAY: {
        size_t re = 0;
        for (auto& i : *value_parray)
            re += i.hashCode();
        return re;
    }
    case JKSN_OBJECT: {
        size_t re = 0;
        for (auto& i : *value_pobject)
            re += i.first.hashCode() + i.second.hashCode();
        return re;
    }
    default:
        return 0;
    }
}
