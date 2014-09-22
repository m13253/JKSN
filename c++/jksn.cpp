#include <cmath>
#include <string>
#include "jksn.hpp"

namespace JKSN {

struct JKSNEncoderImpl {
};

struct JKSNDecoderImpl {
};

JKSNError::JKSNError(const char *reason) noexcept {
    this->reason = reason;
}

const char *JKSNError::what() const noexcept {
    return this->reason;
}

JKSNObject::JKSNObject() {
    this->data_type = JKSN_UNDEFINED;
}

JKSNObject::JKSNObject(bool data) {
    this->data_type = JKSN_BOOL;
    this->data_bool = data;
}

JKSNObject::JKSNObject(int64_t data) {
    this->data_type = JKSN_INT;
    this->data_int = data;
}

JKSNObject::JKSNObject(float data) {
    this->data_type = JKSN_FLOAT;
    this->data_float = data;
}

JKSNObject::JKSNObject(double data) {
    this->data_type = JKSN_DOUBLE;
    this->data_double = data;
}

JKSNObject::JKSNObject(long double data) {
    this->data_type = JKSN_LONG_DOUBLE;
    this->data_long_double = data;
}

JKSNObject::JKSNObject(std::shared_ptr<std::string> data, bool isblob) {
    this->data_type = isblob ? JKSN_BLOB : JKSN_STRING;
    *(this->data_string = new std::shared_ptr<std::string>) = data;
}

JKSNObject::JKSNObject(std::shared_ptr<std::vector<JKSNObject>> data) {
    this->data_type = JKSN_ARRAY;
    *(this->data_array = new std::shared_ptr<std::vector<JKSNObject>>) = data;
}

JKSNObject::JKSNObject(std::shared_ptr<std::map<JKSNObject, JKSNObject>> data) {
    this->data_type = JKSN_OBJECT;
    *(this->data_object = new std::shared_ptr<std::map<JKSNObject, JKSNObject>>) = data;
}

JKSNObject::~JKSNObject() {
    switch(this->getType()) {
    case JKSN_STRING:
    case JKSN_BLOB:
        delete this->data_string;
        break;
    case JKSN_ARRAY:
        delete this->data_array;
        break;
    case JKSN_OBJECT:
        delete this->data_object;
        break;
    default:
        break;
    }
}

jksn_data_type JKSNObject::getType() const {
    return this->data_type;
}

bool JKSNObject::toBool() const {
    switch(this->getType()) {
    case JKSN_UNDEFINED:
    case JKSN_NULL:
    case JKSN_UNSPECIFIED:
        return false;
    case JKSN_BOOL:
        return this->data_bool;
    case JKSN_INT:
        return this->data_int != 0;
    case JKSN_FLOAT:
        return this->data_float != 0.0f;
    case JKSN_DOUBLE:
        return this->data_double != 0.0;
    case JKSN_LONG_DOUBLE:
        return this->data_long_double != 0.0L;
    case JKSN_STRING:
    case JKSN_BLOB:
        return !this->data_string->get()->empty();
    default:
        return true;
    }
}

int64_t JKSNObject::toInt() const {
    switch(this->getType()) {
    case JKSN_INT:
        return this->data_int;
    case JKSN_FLOAT:
        return int64_t(this->data_float);
    case JKSN_DOUBLE:
        return int64_t(this->data_double);
    case JKSN_LONG_DOUBLE:
        return int64_t(this->data_long_double);
    case JKSN_STRING:
        return int64_t(std::stoll(*this->data_string->get()));
    default:
        throw std::invalid_argument("Cannot convert data to the specified type");
    }
}

float JKSNObject::toFloat() const {
    switch(this->getType()) {
    case JKSN_INT:
        return float(this->data_int);
    case JKSN_FLOAT:
        return this->data_float;
    case JKSN_DOUBLE:
        return float(this->data_double);
    case JKSN_LONG_DOUBLE:
        return float(this->data_long_double);
    case JKSN_STRING:
        try {
            return std::stof(*this->data_string->get());
        } catch(std::invalid_argument) {
            return NAN;
        } catch(std::out_of_range) {
            return NAN;
        }
    default:
        return NAN;
    }
}

double JKSNObject::toDouble() const {
    switch(this->getType()) {
    case JKSN_INT:
        return double(this->data_int);
    case JKSN_FLOAT:
        return double(this->data_float);
    case JKSN_DOUBLE:
        return this->data_double;
    case JKSN_LONG_DOUBLE:
        return double(this->data_long_double);
    case JKSN_STRING:
        try {
            return std::stod(*this->data_string->get());
        } catch(std::invalid_argument) {
            return NAN;
        } catch(std::out_of_range) {
            return NAN;
        }
    default:
        return NAN;
    }
}

long double JKSNObject::toLongDouble() const {
    switch(this->getType()) {
    case JKSN_INT:
        return (long double) this->data_int;
    case JKSN_FLOAT:
        return (long double) this->data_float;
    case JKSN_DOUBLE:
        return (long double) this->data_double;
    case JKSN_LONG_DOUBLE:
        return this->data_long_double;
    case JKSN_STRING:
        try {
            return std::stold(*this->data_string->get());
        } catch(std::invalid_argument) {
            return NAN;
        } catch(std::out_of_range) {
            return NAN;
        }
    default:
        return NAN;
    }
}

std::shared_ptr<std::string> JKSNObject::toString() const {
    switch(this->getType()) {
    case JKSN_UNDEFINED:
        return std::make_shared<std::string>(std::string("undefined"));
    case JKSN_NULL:
        return std::make_shared<std::string>(std::string("null"));
    case JKSN_BOOL:
        return std::make_shared<std::string>(std::string(this->data_bool ? "true" : "false"));
    case JKSN_INT:
        return std::make_shared<std::string>(std::to_string(this->data_int));
    case JKSN_FLOAT:
        return std::make_shared<std::string>(std::to_string(this->data_float));
    case JKSN_DOUBLE:
        return std::make_shared<std::string>(std::to_string(this->data_double));
    case JKSN_LONG_DOUBLE:
        return std::make_shared<std::string>(std::to_string(this->data_long_double));
    case JKSN_STRING:
    case JKSN_BLOB:
        return *this->data_string;
    case JKSN_ARRAY:
        {
            std::string res;
            for(size_t i = 0; i < this->data_array->get()->size(); i++) {
                if(i != 0)
                    res.append(1, ',');
                res += *(*this->data_array->get())[i].toString();
            }
            return std::make_shared<std::string>(res);
        }
    default:
        return std::make_shared<std::string>(std::string("[object Object]"));
    }
}

std::shared_ptr<std::string> JKSNObject::toBlob() const {
    if(this->getType() == JKSN_BLOB)
        return *this->data_string;
    else
        throw std::invalid_argument("Cannot convert data to the specified type");
}

std::shared_ptr<std::vector<JKSNObject>> JKSNObject::toArray() const {
    if(this->getType() == JKSN_ARRAY)
        return *this->data_array;
    else
        return std::make_shared<std::vector<JKSNObject>>(std::vector<JKSNObject>(1, *this));
}

std::shared_ptr<std::map<JKSNObject, JKSNObject>> JKSNObject::toObject() const {
    switch(this->getType()) {
    case JKSN_ARRAY:
        {
            std::map<JKSNObject, JKSNObject> res;
            for(size_t i = 0; i < this->data_array->get()->size(); i++)
                res[new JKSNObject(int64_t(i))] = (*this->data_array->get())[i];
            return std::make_shared<std::map<JKSNObject, JKSNObject>>(res);
        }
    case JKSN_OBJECT:
        return *this->data_object;
    default:
        throw std::invalid_argument("Cannot convert data to the specified type");
    }
}

}
