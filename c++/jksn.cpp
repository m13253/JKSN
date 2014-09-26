#include "jksn.hpp"

#include <cmath>
#include <cstdlib>
#include <functional>
#include <sstream>

using namespace JKSN;

JKSNObject::~JKSNObject() {
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

JKSNObject& JKSNObject::operator = (const JKSNObject& rhs) {
    if (value_type == rhs.value_type) {
        switch (value_type) {
        case JKSN_BOOL:
            value_bool = rhs.value_bool;
            return *this;
        case JKSN_INT:
            value_int = rhs.value_int;
            return *this;
        case JKSN_FLOAT:
            value_float = rhs.value_float;
            return *this;
        case JKSN_DOUBLE:
            value_double = rhs.value_double;
            return *this;
        case JKSN_LONG_DOUBLE:
            value_long_double = rhs.value_long_double;
            return *this;
        case JKSN_STRING:
        case JKSN_BLOB:
            if (value_pstr.unique())
                value_pstr = rhs.value_pstr;
            else
                *value_pstr = *rhs.value_pstr;
            return *this;
        case JKSN_ARRAY:
            if (value_parray.unique())
                value_parray = rhs.value_parray;
            else
                *value_parray = *rhs.value_parray;
            return *this;
        case JKSN_OBJECT:
            if (value_pobject.unique())
                value_pobject = rhs.value_pobject;
            else
                *value_pobject = *rhs.value_pobject;
            return *this;
        default:
            return *this;
        }
    }
    else {
        ~JKSNObject();
        value_type = JKSN_UNDEFINED;
        switch (rhs.value_type) {
        case JKSN_BOOL:
            value_bool = rhs.value_bool;
            break;
        case JKSN_INT:
            value_int = rhs.value_int;
            break;
        case JKSN_FLOAT:
            value_float = rhs.value_float;
            break;
        case JKSN_DOUBLE:
            value_double = rhs.value_double;
            break;
        case JKSN_LONG_DOUBLE:
            value_long_double = rhs.value_long_double;
            break;
        case JKSN_STRING:
        case JKSN_BLOB:
            new(&value_pstr) std::shared_ptr<std::string>{new auto{*rhs.value_pstr}};
            break;
        case JKSN_ARRAY:
            new(&value_parray) std::shared_ptr<Array>{new auto{*rhs.value_parray}};
            break;
        case JKSN_OBJECT:
            new(&value_pobject) std::shared_ptr<Object>{new auto{*rhs.value_pobject}};
            break;
        }
        value_type = rhs.value_type;
        return *this;
    }
}

bool JKSNObject::toBool() const {
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
        return !*value_pstr == "";
    default:
        return true;
    }
}

int64_t JKSNObject::toInt() const {
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
        return static_cast<int64_t>(std::atoll((*value_pstr)->c_str()));
    default:
        throw JKSNTypeError{"Cannot convert value to int64_t."};
    }
}

float JKSNObject::toFloat() const {
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
        const char *start = (*value_pstr)->c_str();
        char *end = nullptr;
        float re = std::strtof(start, &end);
        if(re == HUGE_VALF || end != start + (*value_pstr)->size())
            return NAN;
        else
            return re;
    }
    default:
        return NAN;
    }
}

double JKSNObject::toDouble() const {
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
        const char *start = (*value_pstr)->c_str();
        char *end = nullptr;
        double re = std::strtod(start, &end);
        if(re == HUGE_VAL || end != start + (*value_pstr)->size())
            return NAN;
        else
            return re;
    }
    default:
        return NAN;
    }
}

long double JKSNObject::toLongDouble() const {
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
        const char *start = (*value_pstr)->c_str();
        char *end = nullptr;
        long double re = std::strtof(start, &end);
        if(re == HUGE_VALL || end != start + (*value_pstr)->size())
            return NAN;
        else
            return re;
    }
    default:
        return NAN;
    }
}

std::string JKSNObject::toString() const {
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
        bool comma = false;
        for (auto& i : *value_array) {
            if (comma)
                os << ',';
            comma = true;
            os << i.toString();
        }
        return os.str();
    }
    default:
        return "[object Object]";
    }
}

std::string JKSNObject::toBlob() const {
    if (value_type == JKSN_BLOB)
        return *value_pstr;
    else
        throw JKSNTypeError{"Cannot convert value to blob."};
}

JKSNObject::Array JKSNObject::toArray() const {
    if (value_type == JKSN_ARRAY)
        return *value_parray;
    else
        return {*this};
}

JKSNObject::Object JKSNObject::toObject() const {
    switch (value_type) {
    case JKSN_ARRAY: {
        Object res;
        for (size_t i = 0; i < (*value_parray)->size(); i++)
            res[i] = (*value_parray)[i];
        return res;
    }
    case JKSN_OBJECT:
        return *value_pobject;
    default:
        throw JKSNTypeError{"Cannot convert value to object."};
    }
}

bool JKSNObject::operator == (const JKSNObject& rhs) const {
    if (value_type != that.value_type) {
        return false;
    }
    else {
        switch (value_type) {
        case JKSN_BOOL:
        case JKSN_INT:
        case JKSN_FLOAT:
        case JKSN_DOUBLE:
        case JKSN_LONG_DOUBLE:
            return toLongDouble() == that.toLongDouble();
        case JKSN_STRING:
        case JKSN_BLOB:
            return value_pstr == that.value_pstr || *value_pstr == *that.value_pstr;
        case JKSN_ARRAY:
            return value_parray == that.value_parray || *value_parray == *that.value_parray;
        case JKSN_OBJECT:
            return value_pobject == that.value_pobject || *value_pobject == *that.value_pobject;
        default:
            return true;
        }
    }
}

size_t JKSNObject::hashCode() {
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
