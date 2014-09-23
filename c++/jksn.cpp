#include <cstdlib>
#include <cmath>
#include <string>
#include "jksn.hpp"

namespace JKSN {

struct JKSNEncoderImpl {
};

struct JKSNDecoderImpl {
};

JKSNObject::JKSNObject() : data_type{JKSN_UNDEFINED} {}

JKSNObject::JKSNObject(bool data) : data_type{JKSN_BOOL}, data_bool{data} {}

JKSNObject::JKSNObject(int64_t data) : data_type{JKSN_INT}, data_int{data} {}

JKSNObject::JKSNObject(float data) : data_type{JKSN_FLOAT}, data_float{data} {}

JKSNObject::JKSNObject(double data) : data_type{JKSN_DOUBLE}, data_double{data} {}

JKSNObject::JKSNObject(long double data) : data_type{JKSN_LONG_DOUBLE}, data_long_double{data} {}

JKSNObject::JKSNObject(std::shared_ptr<std::string> data, bool isblob) :
        data_type{isblob ? JKSN_BLOB : JKSN_STRING},
        data_string{new auto{data}} {}

JKSNObject::JKSNObject(std::shared_ptr<std::vector<JKSNObject>> data) :
        data_type{JKSN_ARRAY},
        data_array{new auto{data} {}

JKSNObject::JKSNObject(std::shared_ptr<std::map<JKSNObject, JKSNObject>> data) :
        data_type{JKSN_OBJECT},
        data_object{new auto{data} {}

JKSNObject::~JKSNObject() {
    switch(data_type) {
    case JKSN_STRING:
    case JKSN_BLOB:
        delete data_string;
        break;
    case JKSN_ARRAY:
        delete data_array;
        break;
    case JKSN_OBJECT:
        delete data_object;
        break;
    default:
        break;
    }
}

jksn_data_type JKSNObject::data_type const {
    return data_type;
}

bool JKSNObject::toBool() const {
    switch(data_type) {
    case JKSN_UNDEFINED:
    case JKSN_NULL:
    case JKSN_UNSPECIFIED:
        return false;
    case JKSN_BOOL:
        return data_bool;
    case JKSN_INT:
        return data_int != 0;
    case JKSN_FLOAT:
        return data_float != 0.0f;
    case JKSN_DOUBLE:
        return data_double != 0.0;
    case JKSN_LONG_DOUBLE:
        return data_long_double != 0.0L;
    case JKSN_STRING:
    case JKSN_BLOB:
        return !(*data_string)->empty();
    default:
        return true;
    }
}

int64_t JKSNObject::toInt() const {
    switch(data_type) {
    case JKSN_UNDEFINED:
    case JKSN_NULL:
    case JKSN_UNSPECIFIED:
        return 0;
    case JKSN_BOOL:
        return data_bool ? 1 : 0;
    case JKSN_INT:
        return data_int;
    case JKSN_FLOAT:
        return static_cast<int64_t>(data_float);
    case JKSN_DOUBLE:
        return static_cast<int64_t>(data_double);
    case JKSN_LONG_DOUBLE:
        return static_cast<int64_t>(data_long_double);
    case JKSN_STRING:
        return static_cast<int64_t>(std::atoll((*data_string)->c_str()));
    default:
        throw JKSNError{"Cannot convert data to the specified type"};
    }
}

float JKSNObject::toFloat() const {
    switch(data_type) {
    case JKSN_BOOL:
        return data_bool ? 1.0f : 0.0f;
    case JKSN_INT:
        return static_cast<float>(data_int);
    case JKSN_FLOAT:
        return data_float;
    case JKSN_DOUBLE:
        return static_cast<float>(data_double);
    case JKSN_LONG_DOUBLE:
        return static_cast<float>(data_long_double);
    case JKSN_STRING: {
        float re = std::strtof(data_string->c_str());
        if(re == 0 || re == HUGE_VALF)
            return NAN;
        else
            return re;
    }
    default:
        return NAN;
    }
}

double JKSNObject::toDouble() const {
    switch(data_type) {
    case JKSN_BOOL:
        return data_bool ? 1.0 : 0.0;
    case JKSN_INT:
        return static_cast<double>(data_int);
    case JKSN_FLOAT:
        return static_cast<double>(data_float);
    case JKSN_DOUBLE:
        return data_double;
    case JKSN_LONG_DOUBLE:
        return static_cast<double>(data_long_double);
    case JKSN_STRING: {
        double re = std::strtod(data_string->c_str());
        if(re == 0 || re == HUGE_VAL)
            return NAN;
        else
            return re;
    }
    default:
        return NAN;
    }
}

long double JKSNObject::toLongDouble() const {
    switch(data_type) {
    case JKSN_BOOL:
        return data_bool ? 1.0L : 0.0L;
    case JKSN_INT:
        return static_cast<long double>(data_int);
    case JKSN_FLOAT:
        return static_cast<long double>(data_float);
    case JKSN_DOUBLE:
        return static_cast<long double>(data_double);
    case JKSN_LONG_DOUBLE:
        return data_long_double;
    case JKSN_STRING:
        long double re = std::strtold(data_string->c_str());
        if(re == 0 || re == HUGE_VALL)
            return NAN;
        else
            return re;
    default:
        return NAN;
    }
}

std::shared_ptr<std::string> JKSNObject::toString() const {
    std::ostringstream os;
    switch(data_type) {
    case JKSN_UNDEFINED:
        return std::make_shared<std::string>("undefined");
    case JKSN_NULL:
        return std::make_shared<std::string>("null");
    case JKSN_BOOL:
        return std::make_shared<std::string>(data_bool ? "true" : "false");
    case JKSN_INT:
        os << data_int;
        return std::make_shared<std::string>(os.str());
    case JKSN_FLOAT:
        os << data_float;
        return std::make_shared<std::string>(os.str());
    case JKSN_DOUBLE:
        os << data_double;
        return std::make_shared<std::string>(os.str());
    case JKSN_LONG_DOUBLE:
        os << data_long_double;
        return std::make_shared<std::string>(os.str());
    case JKSN_STRING:
    case JKSN_BLOB:
        return *data_string;
    case JKSN_ARRAY:
    {
        std::ostringstream os;
        bool comma = false;
        for(auto &i : **data_array) {
            if(comma)
                os << ',';
            comma = true;
            os << i.toString();
        }
        return std::make_shared<std::string>(os.str());
    }
    default:
        return std::make_shared<std::string>("[object Object]");
    }
}

std::shared_ptr<std::string> JKSNObject::toBlob() const {
    if(data_type == JKSN_BLOB)
        return data_string;
    else
        throw JKSNError{"Cannot convert data to the specified type"};
}

std::shared_ptr<std::vector<JKSNObject>> JKSNObject::toArray() const {
    if(data_type == JKSN_ARRAY)
        return *data_array;
    else
        return std::make_shared<std::vector<JKSNObject>>(1, *this); // FIXME: memory leak! copying unmanaged pointers.
}

std::shared_ptr<std::map<JKSNObject, JKSNObject>> JKSNObject::toObject() const {
    switch(data_type) {
    case JKSN_ARRAY:
    {
        std::map<JKSNObject, JKSNObject> res;
        for(size_t i = 0; i < (*data_array)->size(); i++)
            res[JKSNObject(static_cast<int64_t>(i))] = (**data_array)[i]; // FIXME: memory leak!
        return std::make_shared<std::map<JKSNObject, JKSNObject>>(res);
    }
    case JKSN_OBJECT:
        return *data_object;
    default:
        throw JKSNError{"Cannot convert data to the specified type"};
    }
}

bool JKSNObject::operator==(const JKSNObject &that) const {
    if(data_type != that.data_type)
        switch(data_type) {
        case JKSN_UNDEFINED:
        case JKSN_NULL:
        case JKSN_UNSPECIFIED:
            return true;
        case JKSN_BOOL:
            return toBool() == that.toBool();
        case JKSN_INT:
            return toInt() == that.toInt();
        case JKSN_FLOAT:
            return toFloat() == that.toFloat();
        case JKSN_DOUBLE:
            return toDouble() == that.toDouble();
        case JKSN_LONG_DOUBLE:
            return toLongDouble() == that.toLongDouble();
        case JKSN_STRING:
            return toString() == that.toString();
        case JKSN_BLOB:
            return toBlob() == that.toBlob();
        case JKSN_ARRAY:
            return toArray() == that.toArray();
        case JKSN_OBJECT:
            return toObject() == that.toObject();
        default:
            throw JKSNError{"Invalid data type"};
        }
    else if((data_type == JKSN_FLOAT || data_type == JKSN_DOUBLE || data_type == JKSN_LONG_DOUBLE) &&
            (that.data_type == JKSN_FLOAT || that.data_type == JKSN_DOUBLE || that.data_type == JKSN_LONG_DOUBLE))
        return toLongDouble() == that.toLongDouble();
    else
        return false;
}

bool JKSNObject::operator<(const JKSNObject &that) const {
    if(data_type != that.data_type)
        switch(data_type) {
        case JKSN_UNDEFINED:
        case JKSN_NULL:
        case JKSN_UNSPECIFIED:
            return true;
        case JKSN_BOOL:
            return toBool() < that.toBool();
        case JKSN_INT:
            return toInt() < that.toInt();
        case JKSN_FLOAT:
            return toFloat() < that.toFloat();
        case JKSN_DOUBLE:
            return toDouble() < that.toDouble();
        case JKSN_LONG_DOUBLE:
            return toLongDouble() < that.toLongDouble();
        case JKSN_STRING:
            return toString() < that.toString();
        case JKSN_BLOB:
            return toBlob() < that.toBlob();
        case JKSN_ARRAY:
            return toArray() < that.toArray();
        case JKSN_OBJECT:
            return toObject() < that.toObject();
        default:
            throw JKSNError{"Invalid data type"};
        }
    else if((data_type == JKSN_FLOAT || data_type == JKSN_DOUBLE || data_type == JKSN_LONG_DOUBLE) &&
            (that.data_type == JKSN_FLOAT || that.data_type == JKSN_DOUBLE || that.data_type == JKSN_LONG_DOUBLE))
        return toLongDouble() < that.toLongDouble();
    else
        return data_type < that.data_type;
}

std::shared_ptr<JKSNObject> JKSNObject::operator[](const JKSNObject &key) const {
    if(data_type == JKSN_OBJECT || key.data_type != JKSN_INT)
        return std::make_shared<JKSNObject>((*toObject())[key]); // FIXME: the implementation of std::shared_ptr is not intrusive, so again, you must write a copy constructor. Otherwise you can make the key type of std::map a std::shared_ptr.
    else
        return std::make_shared<JKSNObject>((*toArray())[key.toInt()]);
}

std::shared_ptr<JKSNObject> JKSNObject::operator[](size_t key) const {
    if(data_type == JKSN_OBJECT)
        return std::make_shared<JKSNObject>((*toObject())[JKSNObject(static_cast<int64_t>(key))]);
    else
        return std::make_shared<JKSNObject>((*toArray())[key]);
}

std::shared_ptr<JKSNObject> JKSNObject::operator[](const std::string &key) const {
    return std::make_shared<JKSNObject>((*toObject())[JKSNObject(std::make_shared<std::string>(key))]);
}

std::shared_ptr<JKSNObject> JKSNObject::operator[](const char *key) const {
    return (*this)[std::string(key)];
}

}
