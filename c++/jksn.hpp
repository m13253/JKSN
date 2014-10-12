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

#ifndef _JKSN_HPP
#define _JKSN_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace JKSN {

class JKSNError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};
class JKSNEncodeError : public JKSNError {
    using JKSNError::JKSNError;
};
class JKSNDecodeError : public JKSNError {
    using JKSNError::JKSNError;
};
class JKSNChecksumError : public JKSNDecodeError {
    using JKSNDecodeError::JKSNDecodeError;
public:
    JKSNChecksumError() : JKSNDecodeError("JKSN stream corrupted") {}
};
class JKSNTypeError : public JKSNDecodeError {
    using JKSNDecodeError::JKSNDecodeError;
public:
    JKSNTypeError() : JKSNDecodeError("invalid JKSN data type") {}
};

typedef enum {
    JKSN_UNDEFINED,
    JKSN_NULL,
    JKSN_BOOL,
    JKSN_INT,
    JKSN_FLOAT,
    JKSN_DOUBLE,
    JKSN_LONG_DOUBLE,
    JKSN_STRING,
    JKSN_BLOB,
    JKSN_ARRAY,
    JKSN_OBJECT,
    JKSN_UNSPECIFIED
} jksn_data_type;

class Unspecified {};

class JKSNValue {
public:
    JKSNValue() : data_type(JKSN_UNDEFINED) {}
    JKSNValue(const JKSNValue &that);
    JKSNValue(std::nullptr_t data) : data_type(JKSN_NULL) {
        if(data != nullptr)
            throw std::invalid_argument("invalid JKSN value");
    }
    JKSNValue(bool data) : data_type(JKSN_BOOL), data_bool(data) {}
    JKSNValue(intmax_t data) : data_type(JKSN_INT), data_int(data) {}
    JKSNValue(uintmax_t data) : data_type(JKSN_INT), data_int(static_cast<intmax_t>(data)) {
        if(this->data_int < 0)
            throw std::overflow_error("JKSN value too large");
    }
    JKSNValue(float data) : data_type(JKSN_FLOAT), data_float(data) {}
    JKSNValue(double data) : data_type(JKSN_DOUBLE), data_double(data) {}
    JKSNValue(long double data) : data_type(JKSN_LONG_DOUBLE), data_long_double(data) {}
    JKSNValue(const std::string &data, bool is_blob = false) : data_type(is_blob ? JKSN_BLOB : JKSN_STRING), data_string(new std::string(data)) {}
    JKSNValue(std::string &&data, bool is_blob = false) : data_type(is_blob ? JKSN_BLOB : JKSN_STRING), data_string(new std::string(data)) {}
    JKSNValue(const std::vector<JKSNValue> &data) : data_type(JKSN_ARRAY), data_array(new std::vector<JKSNValue>(data)) {}
    JKSNValue(std::vector<JKSNValue> &&data) : data_type(JKSN_ARRAY), data_array(new std::vector<JKSNValue>(data)) {}
    JKSNValue(std::initializer_list<JKSNValue> data) : data_type(JKSN_ARRAY), data_array(new std::vector<JKSNValue>(data)) {}
    JKSNValue(const std::map<JKSNValue, JKSNValue> &data) : data_type(JKSN_OBJECT), data_object(new std::map<JKSNValue, JKSNValue>(data)) {}
    JKSNValue(std::map<JKSNValue, JKSNValue> &&data) : data_type(JKSN_OBJECT), data_object(new std::map<JKSNValue, JKSNValue>(data)) {}
    JKSNValue(std::initializer_list<std::pair<const JKSNValue, JKSNValue> > data) : data_type(JKSN_OBJECT), data_object(new std::map<JKSNValue, JKSNValue>(data)) {}
    JKSNValue(const Unspecified &) : data_type(JKSN_UNSPECIFIED) {}
    ~JKSNValue();

    JKSNValue &operator[](size_t index) {
        switch(this->getType()) {
        case JKSN_ARRAY:
            return static_cast<std::vector<JKSNValue> &>(*this)[index];
        case JKSN_OBJECT:
            return static_cast<std::map<JKSNValue, JKSNValue> &>(*this)[JKSNValue(index)];
        default:
            throw JKSNTypeError();
        }
    }
    const JKSNValue &operator[](size_t index) const {
        switch(this->getType()) {
        case JKSN_ARRAY:
            return static_cast<const std::vector<JKSNValue> &>(*this)[index];
        case JKSN_OBJECT:
            return static_cast<const std::map<JKSNValue, JKSNValue> &>(*this)[JKSNValue(index)];
        default:
            throw JKSNTypeError();
        }
    }
    bool operator<(const JKSNValue &that) const;
    bool operator==(const JKSNValue &that) const;

    jksn_data_type getType() const { return this->data_type; }
    bool isUndefined() const { return this->getType() == JKSN_UNDEFINED; }
    bool isNull() const { return this->getType() == JKSN_UNDEFINED; }
    bool isBool() const { return this->getType() == JKSN_NULL; }
    bool isInt() const { return this->getType() == JKSN_INT; }
    bool isFloat() const { return this->getType() == JKSN_FLOAT; }
    bool isDouble() const { return this->getType() == JKSN_DOUBLE; }
    bool isLongDouble() const { return this->getType() == JKSN_LONG_DOUBLE; }
    bool isNumber() const {
        jksn_data_type type = this->getType();
        return type == JKSN_INT || type == JKSN_FLOAT || type == JKSN_DOUBLE || type == JKSN_LONG_DOUBLE;
    }
    bool isString() const { return this->getType() == JKSN_STRING; }
    bool isBlob() const { return this->getType() == JKSN_BLOB; }
    bool isStringOrBlob() const {
        jksn_data_type type = this->getType();
        return type == JKSN_STRING || type == JKSN_BLOB;
    }
    bool isArray() const { return this->getType() == JKSN_ARRAY; }
    bool isObject() const { return this->getType() == JKSN_OBJECT; }
    bool isContainer() const {
        jksn_data_type type = this->getType();
        return type == JKSN_ARRAY || type == JKSN_OBJECT;
    }
    bool isIterable() const {
        jksn_data_type type = this->getType();
        return type == JKSN_STRING || type == JKSN_BLOB || type == JKSN_ARRAY || type == JKSN_OBJECT;
    }
    bool isUnspecified() const { return this->getType() == JKSN_UNSPECIFIED; }

    explicit operator std::nullptr_t() const {
        if(this->isNull()) return nullptr; else throw JKSNTypeError();
    };
    explicit operator bool() const;
    explicit operator intmax_t() const;
    explicit operator float() const { return this->toNumber<float>(); }
    explicit operator double() const { return this->toNumber<double>(); }
    explicit operator long double() const { return this->toNumber<long double>(); }
    explicit operator std::string() const;
    explicit operator const std::vector<JKSNValue> &() const {
        if(this->isArray()) return *this->data_array; else throw JKSNTypeError();
    }
    explicit operator std::vector<JKSNValue> &() {
        if(this->isArray()) return *this->data_array; else throw JKSNTypeError();
    }
    explicit operator std::vector<JKSNValue>() const {
        if(this->isArray()) return *this->data_array; else throw JKSNTypeError();
    }
    explicit operator const std::map<JKSNValue, JKSNValue> &() const {
        if(this->isObject()) return *this->data_object; else throw JKSNTypeError();
    }
    explicit operator std::map<JKSNValue, JKSNValue> &() {
        if(this->isObject()) return *this->data_object; else throw JKSNTypeError();
    }
    explicit operator std::map<JKSNValue, JKSNValue>() const {
        if(this->isObject()) return *this->data_object; else throw JKSNTypeError();
    }
    explicit operator Unspecified() const { return Unspecified(); }

private:
    jksn_data_type data_type = JKSN_UNDEFINED;
    union {
        bool data_bool;
        intmax_t data_int;
        float data_float;
        double data_double;
        long double data_long_double;
        std::string *data_string;
        std::vector<JKSNValue> *data_array;
        std::map<JKSNValue, JKSNValue> *data_object;
    };

    template<typename T> inline T toNumber() const;
};


}

#endif
