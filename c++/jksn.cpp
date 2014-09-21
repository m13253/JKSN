#include "jksn.hpp"

namespace JKSN {

struct JKSNObjectImpl {
    jksn_data_type data_type;
    union {
        int data_bool;
        int64_t data_int;
        float data_float;
        double data_double;
        long double data_long_double;
        std::string *data_string;
        std::vector<JKSNObject> *data_array;
        std::map<JKSNObject, JKSNObject> *data_object;
    };
};

struct JKSNEncoderImpl {
};

struct JKSNDecoderImpl {
};

JKSNError::JKSNError(const char *reason) throw() {
    this->reason = reason;
}

const char *JKSNError::what() const throw() {
    return this->reason;
}

void JKSNObject::init() {
    this->impl = new struct JKSNObjectImpl;
}

JKSNObject::JKSNObject() {
    this->init();
    this->impl->data_type = JKSN_UNDEFINED;
}

JKSNObject::JKSNObject(bool data) {
    this->init();
    this->impl->data_type = JKSN_BOOL;
    this->impl->data_bool = data;
}

JKSNObject::JKSNObject(int64_t data) {
    this->init();
    this->impl->data_type = JKSN_INT;
    this->impl->data_int = data;
}

JKSNObject::JKSNObject(float data) {
    this->init();
    this->impl->data_type = JKSN_FLOAT;
    this->impl->data_float = data;
}

JKSNObject::JKSNObject(double data) {
    this->init();
    this->impl->data_type = JKSN_DOUBLE;
    this->impl->data_double = data;
}

JKSNObject::JKSNObject(long double data) {
    this->init();
    this->impl->data_type = JKSN_LONG_DOUBLE;
    this->impl->data_long_double = data;
}

JKSNObject::JKSNObject(std::string &data, bool isblob) {
    this->init();
    this->impl->data_type = isblob ? JKSN_BLOB : JKSN_STRING;
    this->impl->data_string = &data;
}

JKSNObject::JKSNObject(std::vector<JKSNObject> &data) {
    this->init();
    this->impl->data_type = JKSN_ARRAY;
    this->impl->data_array = &data;
}

JKSNObject::JKSNObject(std::map<JKSNObject, JKSNObject> &data) {
    this->init();
    this->impl->data_type = JKSN_OBJECT;
    this->impl->data_object = &data;
}

JKSNObject::~JKSNObject() {
    delete this->impl;
}

jksn_data_type JKSNObject::getType() const {
    return this->impl->data_type;
}

bool JKSNObject::toBool() const {
    switch(this->getType()) {
    case JKSN_UNDEFINED:
    case JKSN_NULL:
    case JKSN_UNSPECIFIED:
        return false;
    case JKSN_BOOL:
        return this->impl->data_bool;
    case JKSN_INT:
        return this->impl->data_int != 0;
    case JKSN_FLOAT:
        return this->impl->data_float != 0.0f;
    case JKSN_DOUBLE:
        return this->impl->data_double != 0.0;
    case JKSN_LONG_DOUBLE:
        return this->impl->data_long_double != 0.0L;
    case JKSN_STRING:
    case JKSN_BLOB:
        return !this->impl->data_string->empty();
    default:
        return true;
    }
}

int64_t JKSNObject::toInt() const {
    switch(this->getType()) {
    case JKSN_INT:
        return this->impl->data_int;
    case JKSN_FLOAT:
        return int64_t(this->impl->data_float);
    case JKSN_DOUBLE:
        return int64_t(this->impl->data_double);
    case JKSN_LONG_DOUBLE:
        return int64_t(this->impl->data_long_double);
    default:
        return 0;
    }
}

}
