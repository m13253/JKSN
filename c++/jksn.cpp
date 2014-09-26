#include "jksn.hpp"

#include <cmath>
#include <cstdlib>
#include <sstream>

using namespace JKSN;

JKSNObject::~JKSNObject() {
    switch(value_type) {
    case JKSN_STRING:
    case JKSN_BLOB:
        value_pstr.~shared_ptr();
        break;
    case JKSN_ARRAY:
        value_parray.~vector();
        break;
    case JKSN_OBJECT:
        value_pobject.~map();
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
            if (value_pstr->use_count() == 1)
                value_pstr = rhs.value_pstr;
            else
                *value_pstr = *rhs.value_pstr;
            return *this;
        case JKSN_ARRAY:
            if (value_parray->use_count() == 1)
                value_parray = rhs.value_parray;
            else
                *value_parray = *rhs.value_parray;
            return *this;
        case JKSN_OBJECT:
            if (value_pobject->use_count() == 1)
                value_pobject = rhs.value_pobject;
            else
                *value_pobject = *rhs.value_pobject;
            return *this;
        default:
            return *this;
        }
    }
    else {
        // TODO
    }

bool JKSNObject::toBool() const {
    switch(value_type) {
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
    switch(value_type) {
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
        throw JKSNError{"Cannot convert value to int64_t."};
    }
}

float JKSNObject::toFloat() const {
    switch(value_type) {
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
    switch(value_type) {
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
    switch(value_type) {
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
    switch(value_type) {
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
    case JKSN_ARRAY:
    {
        std::ostringstream os;
        bool comma = false;
        for(auto &i : *value_array) {
            if(comma)
                os << ',';
            comma = true;
            os << i.toString();
        }
        return std::make_shared<std::string>(os.str());
    }
    default:
        return "[object Object]";
    }
}

std::string JKSNObject::toBlob() const {
    if(value_type == JKSN_BLOB)
        return *value_pstr;
    else
        throw JKSNError{"Cannot convert value to blob."};
}

JKSNObject::Array JKSNObject::toArray() const {
    if(value_type == JKSN_ARRAY)
        return *value_parray;
    else
        return {*this,};
}

JKSNObject::Object JKSNObject::toObject() const {
    switch(value_type) {
    case JKSN_ARRAY:
    {
        Object res;
        for(size_t i = 0; i < (*value_parray)->size(); i++)
            res[i] = (*value_parray)[i];
        return res;
    }
    case JKSN_OBJECT:
        return *value_pobject;
    default:
        throw JKSNError{"Cannot convert value to object."};
    }
}

bool JKSNObject::operator == (const JKSNObject& rhs) const {
    if (value_type != that.value_type) {
        switch(value_type) {
        case JKSN_BOOL:
            return value_bool == that.toBool();
        case JKSN_INT:
            return value_int == that.toInt();
        case JKSN_FLOAT:
            return value_float == that.toFloat();
        case JKSN_DOUBLE:
            return value_double == that.toDouble();
        case JKSN_LONG_DOUBLE:
            return value_long_double == that.toLongDouble();
        case JKSN_STRING:
            return *value_pstr == that.toString();
        case JKSN_BLOB:
            return *value_pstr == that.toBlob();
        case JKSN_ARRAY:
            return *value_parray == that.toArray();
        case JKSN_OBJECT:
            return *value_pobject == that.toObject();
        default:
            return false;
        }
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
            return value_pstr = that.value_pstr || *value_pstr == *that.value_pstr;
        case JKSN_ARRAY:
            return value_parray = that.value_parray || *value_parray == *that.value_parray;
        case JKSN_OBJECT:
            return value_pobject = that.value_pobject || *value_pobject == *that.value_pobject;
        default:
            return true;
        }
    }
}

// bool JKSNObject::operator<(const JKSNObject &that) const {
//     if(value_type != that.value_type)
//         switch(value_type) {
//         case JKSN_UNDEFINED:
//         case JKSN_NULL:
//         case JKSN_UNSPECIFIED:
//             return true;
//         case JKSN_BOOL:
//             return toBool() < that.toBool();
//         case JKSN_INT:
//             return toInt() < that.toInt();
//         case JKSN_FLOAT:
//             return toFloat() < that.toFloat();
//         case JKSN_DOUBLE:
//             return toDouble() < that.toDouble();
//         case JKSN_LONG_DOUBLE:
//             return toLongDouble() < that.toLongDouble();
//         case JKSN_STRING:
//             return toString() < that.toString();
//         case JKSN_BLOB:
//             return toBlob() < that.toBlob();
//         case JKSN_ARRAY:
//             return toArray() < that.toArray();
//         case JKSN_OBJECT:
//             return toObject() < that.toObject();
//         default:
//             throw JKSNError{"Invalid data type"};
//         }
//     else if((value_type == JKSN_FLOAT || value_type == JKSN_DOUBLE || value_type == JKSN_LONG_DOUBLE) &&
//             (that.value_type == JKSN_FLOAT || that.value_type == JKSN_DOUBLE || that.value_type == JKSN_LONG_DOUBLE))
//         return toLongDouble() < that.toLongDouble();
//     else
//         return value_type < that.value_type;
// }
// 
// std::shared_ptr<JKSNObject> JKSNObject::operator[](const JKSNObject &key) const {
//     if(value_type == JKSN_OBJECT || key.value_type != JKSN_INT)
//         return std::make_shared<JKSNObject>((*toObject())[key]); // FIXME: the implementation of std::shared_ptr is not intrusive, so again, you must write a copy constructor. Otherwise you can make the key type of std::map a std::shared_ptr.
//     else
//         return std::make_shared<JKSNObject>((*toArray())[key.toInt()]);
// }
// 
// std::shared_ptr<JKSNObject> JKSNObject::operator[](size_t key) const {
//     if(value_type == JKSN_OBJECT)
//         return std::make_shared<JKSNObject>((*toObject())[JKSNObject(static_cast<int64_t>(key))]);
//     else
//         return std::make_shared<JKSNObject>((*toArray())[key]);
// }
// 
// std::shared_ptr<JKSNObject> JKSNObject::operator[](const std::string &key) const {
//     return std::make_shared<JKSNObject>((*toObject())[JKSNObject(std::make_shared<std::string>(key))]);
// }
