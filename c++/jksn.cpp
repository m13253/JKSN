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
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace JKSN {

class JKSNUnicodeError : public JKSNError {
    using JKSNError::JKSNError;
};

class JKSNProxy {
public:
    JKSNProxy() = delete;
    JKSNProxy(const JKSNValue *origin, uint8_t control, const std::string &data, const std::string &buf) :
        origin(origin),
        control(control),
        data(data),
        buf(buf) {
    }
    JKSNProxy(const JKSNValue *origin, uint8_t control, const std::string &data, std::string &&buf = std::string()) :
        origin(origin),
        control(control),
        data(data),
        buf(std::move(buf)) {
    }
    JKSNProxy(const JKSNValue *origin, uint8_t control, std::string &&data = std::string(), std::string &&buf = std::string()) :
        origin(origin),
        control(control),
        data(std::move(data)),
        buf(std::move(buf)) {
    }
    JKSNProxy(const JKSNValue *origin, uint8_t control, std::string &&data, const std::string &buf) :
        origin(origin),
        control(control),
        data(std::move(data)),
        buf(buf) {
    }
    std::ostream &output(std::ostream &stream, bool recursive = true) const {
        stream.put(char(this->control));
        stream << this->data
               << this->buf;
        if(recursive)
            for(const JKSNProxy &i : this->children)
                i.output(stream);
        return stream;
    }
    std::string toString(bool recursive = true) const {
        std::string result;
        result.reserve(1 + this->data.size() + this->buf.size());
        result += char(this->control);
        result += this->data;
        result += this->buf;
        if(recursive)
            for(const JKSNProxy &i : this->children)
                result += i.toString();
        return result;
    }
    size_t size(size_t depth = 0) const {
        size_t result = 1 + this->data.size() + this->buf.size();
        if(depth == 0)
            for(const JKSNProxy &i : this->children)
                result += i.size();
        else if(depth != 1)
            for(const JKSNProxy &i : this->children)
                result += i.size(depth-1);
        return result;
    }
    const JKSNValue *origin = nullptr; /* weak reference */
    uint8_t control;
    std::string data;
    std::string buf;
    std::vector<JKSNProxy> children;
    uint16_t hash = 0;
};

class JKSNCache {
public:
    bool haslastint = false;
    intmax_t lastint;
    std::shared_ptr<std::string> texthash[256] = {nullptr};
    std::shared_ptr<std::string> blobhash[256] = {nullptr};
};

class JKSNEncoderPrivate {
public:
    JKSNProxy dumpToProxy(const JKSNValue &obj);
private:
    JKSNCache cache;
    static JKSNProxy dumpValue(const JKSNValue &obj);
    static JKSNProxy dumpUndefined(const JKSNValue &obj);
    static JKSNProxy dumpNull(const JKSNValue &obj);
    static JKSNProxy dumpBool(const JKSNValue &obj);
    static JKSNProxy dumpInt(const JKSNValue &obj);
    static std::string encodeInt(uintmax_t number, size_t size);
    static JKSNProxy dumpFloat(const JKSNValue &obj);
    static JKSNProxy dumpDouble(const JKSNValue &obj);
    static JKSNProxy dumpLongDouble(const JKSNValue &obj);
    static JKSNProxy dumpString(const JKSNValue &obj);
    static JKSNProxy dumpBlob(const JKSNValue &obj);
    static JKSNProxy dumpArray(const JKSNValue &obj);
    static JKSNProxy textSwapAvailability(const JKSNValue &obj);
    static JKSNProxy encodeStraightArray(const JKSNValue &obj);
    static JKSNProxy encodeSwappedList(const JKSNValue &obj);
    static JKSNProxy dumpObject(const JKSNValue &obj);
    static JKSNProxy dumpUnspecified(const JKSNValue &obj);
    JKSNProxy &optimize(JKSNProxy &obj);
};

class JKSNDecoderPrivate {
public:
    JKSNValue parseValue(std::istream &fp);
private:
    JKSNCache cache;
};

static inline bool isLittleEndian();

JKSNEncoder::JKSNEncoder() :
    p(new JKSNEncoderPrivate) {
}

JKSNEncoder::JKSNEncoder(const JKSNEncoder &that) :
    p(new JKSNEncoderPrivate(*that.p)) {
}

JKSNEncoder::JKSNEncoder(JKSNEncoder &&that) : p(that.p) {
    that.p = nullptr;
}

JKSNEncoder &JKSNEncoder::operator=(const JKSNEncoder &that) {
    if(this != &that)
        *this->p = *that.p;
    return *this;
}

JKSNEncoder &JKSNEncoder::operator=(JKSNEncoder &&that) {
    if(this != &that) {
        delete this->p;
        this->p = that.p;
        that.p = nullptr;
    }
    return *this;
}

JKSNEncoder::~JKSNEncoder() {
    delete p;
}

std::ostream &JKSNEncoder::dump(std::ostream &result, const JKSNValue &obj, bool header) {
    JKSNProxy proxy = this->p->dumpToProxy(obj);
    if(header)
        result.write("jk!", 3);
    proxy.output(result);
    return result;
}

std::string JKSNEncoder::dumps(const JKSNValue &obj, bool header) {
    std::ostringstream result;
    this->dump(result, obj, header);
    return result.str();
}

JKSNProxy JKSNEncoderPrivate::dumpToProxy(const JKSNValue &obj) {
    JKSNProxy proxy = this->dumpValue(obj);
    this->optimize(proxy);
    return proxy;
}

JKSNProxy &optimize(JKSNProxy &obj) {
    return obj;
}

JKSNProxy JKSNEncoderPrivate::dumpValue(const JKSNValue &obj) {
    switch(obj.getType()) {
    case JKSN_UNDEFINED:
        return dumpUndefined(obj);
    case JKSN_NULL:
        return dumpNull(obj);
    case JKSN_BOOL:
        return dumpBool(obj);
    case JKSN_INT:
        return dumpInt(obj);
    case JKSN_FLOAT:
        return dumpFloat(obj);
    case JKSN_DOUBLE:
        return dumpDouble(obj);
    case JKSN_LONG_DOUBLE:
        return dumpLongDouble(obj);
    case JKSN_STRING:
        return dumpString(obj);
    case JKSN_BLOB:
        return dumpBlob(obj);
    case JKSN_ARRAY:
        return dumpArray(obj);
    case JKSN_OBJECT:
        return dumpObject(obj);
    case JKSN_UNSPECIFIED:
        return dumpUnspecified(obj);
    default:
        throw JKSNEncodeError("cannot encode unrecognizable type of value");
    }
}

JKSNProxy JKSNEncoderPrivate::dumpUndefined(const JKSNValue &obj) {
    return JKSNProxy(&obj, 0x00);
}

JKSNProxy JKSNEncoderPrivate::dumpNull(const JKSNValue &obj) {
    return JKSNProxy(&obj, 0x01);
}

JKSNProxy JKSNEncoderPrivate::dumpBool(const JKSNValue &obj) {
    return JKSNProxy(&obj, obj.toBool() ? 0x03 : 0x02);
}

JKSNProxy JKSNEncoderPrivate::dumpInt(const JKSNValue &obj) {
    const intmax_t number = obj.toInt();
    if(number >= 0 && number <= 0xa)
        return JKSNProxy(&obj, 0x10 | uint8_t(number));
    else if(number >= -0x80 && number <= 0x7f)
        return JKSNProxy(&obj, 0x1d, encodeInt(uintmax_t(number), 1));
    else if(number >= -0x8000 && number <= 0x7fff)
        return JKSNProxy(&obj, 0x1c, encodeInt(uintmax_t(number), 2));
    else if((number >= -0x80000000L && number <= -0x200000L) ||
            (number >= 0x200000L && number <= 0x7fffffffL))
        return JKSNProxy(&obj, 0x1b, encodeInt(uintmax_t(number), 4));
    else if(number >= 0)
        return JKSNProxy(&obj, 0x1f, encodeInt(uintmax_t(number), 0));
    else
        return JKSNProxy(&obj, 0x1e, encodeInt(uintmax_t(-number), 0));
}

JKSNProxy JKSNEncoderPrivate::dumpFloat(const JKSNValue &obj) {
    const float number = obj.toFloat();
    if(isnan(number))
        return JKSNProxy(&obj, 0x20);
    else if(isinf(number))
        return JKSNProxy(&obj, number >= 0 ? 0x2f : 0x2e);
    else {
        static_assert(sizeof (float) == 4, "sizeof (float) should be 4");
        const union {
            float data_float;
            char data_int[4];
        } conv = {number};
        if(isLittleEndian())
            return JKSNProxy(&obj, 0x2d, std::string({
                conv.data_int[3], conv.data_int[2], conv.data_int[1], conv.data_int[0]
            }));
        else
            return JKSNProxy(&obj, 0x2d, std::string({
                conv.data_int[0], conv.data_int[1], conv.data_int[2], conv.data_int[3]
            }));
    }
}

JKSNProxy JKSNEncoderPrivate::dumpDouble(const JKSNValue &obj) {
    const double number = obj.toDouble();
    if(isnan(number))
        return JKSNProxy(&obj, 0x20);
    else if(isinf(number))
        return JKSNProxy(&obj, number >= 0 ? 0x2f : 0x2e);
    else {
        static_assert(sizeof (double) == 8, "sizeof (double) should be 8");
        const union {
            double data_double;
            char data_int[8];
        } conv = {number};
        if(isLittleEndian())
            return JKSNProxy(&obj, 0x2c, std::string({
                conv.data_int[7], conv.data_int[6], conv.data_int[5], conv.data_int[4],
                conv.data_int[3], conv.data_int[2], conv.data_int[1], conv.data_int[0]
            }));
        else
            return JKSNProxy(&obj, 0x2c, std::string({
                conv.data_int[0], conv.data_int[1], conv.data_int[2], conv.data_int[3],
                conv.data_int[4], conv.data_int[5], conv.data_int[6], conv.data_int[7]
            }));
    }
}

JKSNProxy JKSNEncoderPrivate::dumpLongDouble(const JKSNValue &obj) {
    const long double number = obj.toLongDouble();
    if(isnan(number))
        return JKSNProxy(&obj, 0x20);
    else if(isinf(number))
        return JKSNProxy(&obj, number >= 0 ? 0x2f : 0x2e);
    else if(sizeof (long double) == 12) {
        const union {
            long double data_long_double;
            char data_int[12];
        } conv = {number};
        if(isLittleEndian())
            return JKSNProxy(&obj, 0x2b, std::string({
                conv.data_int[9], conv.data_int[8],
                conv.data_int[7], conv.data_int[6], conv.data_int[5], conv.data_int[4],
                conv.data_int[3], conv.data_int[2], conv.data_int[1], conv.data_int[0]
            }));
        else
            return JKSNProxy(&obj, 0x2b, std::string({
                conv.data_int[2], conv.data_int[3],
                conv.data_int[4], conv.data_int[5], conv.data_int[6], conv.data_int[7],
                conv.data_int[8], conv.data_int[9], conv.data_int[10], conv.data_int[11]
            }));
    } else if(sizeof (long double) == 16) {
        const union {
            long double data_long_double;
            char data_int[16];
        } conv = {number};
        if(isLittleEndian())
            return JKSNProxy(&obj, 0x2b, std::string({
                conv.data_int[9], conv.data_int[8],
                conv.data_int[7], conv.data_int[6], conv.data_int[5], conv.data_int[4],
                conv.data_int[3], conv.data_int[2], conv.data_int[1], conv.data_int[0]
            }));
        else
            return JKSNProxy(&obj, 0x2b, std::string({
                conv.data_int[6], conv.data_int[7],
                conv.data_int[8], conv.data_int[9], conv.data_int[10], conv.data_int[11],
                conv.data_int[12], conv.data_int[13], conv.data_int[14], conv.data_int[15]
            }));
    } else
        throw JKSNEncodeError("this build of JKSN decoder does not support long double numbers");
}

JKSNProxy JKSNEncoderPrivate::dumpUnspecified(const JKSNValue &obj) {
    return JKSNProxy(&obj, 0xa0);
}

std::string JKSNEncoderPrivate::encodeInt(uintmax_t number, size_t size) {
    switch(size) {
    case 1:
        return std::string({
            char(uint8_t(number))
        });
    case 2:
        return std::string({
            char(uint8_t(number) >> 8),
            char(uint8_t(number))
        });
    case 4:
        return std::string({
            char(uint8_t(number) >> 24),
            char(uint8_t(number) >> 16),
            char(uint8_t(number) >> 8),
            char(uint8_t(number))
        });
    case 0:
        {
            std::string result(1, char(number & 0x7f));
            number >>= 7;
            while(number != 0) {
                result.append(1, char((number & 0x7f) | 0x80));
                number >>= 7;
            }
            return std::string(result.crbegin(), result.crend());
        }
    default:
        assert(size == 1 || size == 2 || size == 4 || size == 0);
        abort();
    }
}

JKSNDecoder::JKSNDecoder() :
    p(new JKSNDecoderPrivate) {
}

JKSNDecoder::JKSNDecoder(const JKSNDecoder &that) :
    p(new JKSNDecoderPrivate(*that.p)) {
}

JKSNDecoder::JKSNDecoder(JKSNDecoder &&that) :
    p(that.p) {
    that.p = nullptr;
}

JKSNDecoder &JKSNDecoder::operator=(const JKSNDecoder &that) {
    if(this != &that)
        *this->p = *that.p;
    return *this;
}

JKSNDecoder &JKSNDecoder::operator=(JKSNDecoder &&that) {
    if(this != &that) {
        delete this->p;
        this->p = that.p;
        that.p = nullptr;
    }
    return *this;
}

JKSNDecoder::~JKSNDecoder() {
    delete p;
}

JKSNValue JKSNDecoder::parse(std::istream &fp, bool header) {
    (void) fp; (void) header;
    return JKSNValue();
}

JKSNValue JKSNDecoder::parse(const std::string &str, bool header) {
    std::istringstream stream(str);
    return this->parse(stream, header);
}

static inline bool isLittleEndian() {
    static const union {
        uint16_t word;
        uint8_t byte;
    } endiantest = {1};
    return endiantest.byte == 1;
}

bool JKSNValue::toBool() const {
    switch(this->getType()) {
    case JKSN_BOOL:
        return this->data_bool;
    case JKSN_UNDEFINED:
    case JKSN_NULL:
        return false;
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
        return this->data_string->size() != 0;
    case JKSN_ARRAY:
        return this->data_array->size() != 0;
    case JKSN_OBJECT:
        return this->data_object->size() != 0;
    default:
        throw JKSNTypeError();
    }
};

intmax_t JKSNValue::toInt() const {
    switch(this->getType()) {
    case JKSN_INT:
        return this->data_int;
    case JKSN_BOOL:
        return this->data_bool;
    case JKSN_FLOAT:
        return this->data_float;
    case JKSN_DOUBLE:
        return this->data_double;
    case JKSN_LONG_DOUBLE:
        return this->data_long_double;
    case JKSN_NULL:
        return 0;
    case JKSN_STRING:
        try {
            return std::stoll(*this->data_string);
        } catch(std::invalid_argument) {
            throw JKSNTypeError();
        } catch(std::out_of_range) {
            throw JKSNTypeError();
        }
    default:
        throw JKSNTypeError();
    }
}

template<typename T>
T JKSNValue::toNumber() const {
    switch(this->getType()) {
    case JKSN_FLOAT:
        return this->data_float;
    case JKSN_DOUBLE:
        return this->data_double;
    case JKSN_LONG_DOUBLE:
        return this->data_long_double;
    case JKSN_INT:
        return this->data_int;
    case JKSN_BOOL:
        return this->data_bool;
    case JKSN_NULL:
        return 0;
    case JKSN_STRING:
        try {
            return std::stoll(*this->data_string);
        } catch(std::invalid_argument) {
            return NAN;
        } catch(std::out_of_range) {
            return NAN;
        }
    default:
        return NAN;
    }
}

std::string JKSNValue::toString() const {
    switch(this->getType()) {
    case JKSN_UNDEFINED:
        return "undefined";
    case JKSN_NULL:
        return "null";
    case JKSN_BOOL:
        return this->data_bool ? "true" : "false";
    case JKSN_INT:
        return std::to_string(this->data_int);
    case JKSN_FLOAT:
        if(std::isnan(this->data_float))
            return "NaN";
        else if(std::isinf(this->data_float))
            return this->data_float >= 0 ? "Infinity" : "-Infinity";
        else
            return std::to_string(this->data_float);
    case JKSN_DOUBLE:
        if(std::isnan(this->data_double))
            return "NaN";
        else if(std::isinf(this->data_double))
            return this->data_double >= 0 ? "Infinity" : "-Infinity";
        else
            return std::to_string(this->data_double);
    case JKSN_LONG_DOUBLE:
        if(std::isnan(this->data_long_double))
            return "NaN";
        else if(std::isinf(this->data_long_double))
            return this->data_long_double >= 0 ? "Infinity" : "-Infinity";
        else
            return std::to_string(this->data_long_double);
    case JKSN_STRING:
    case JKSN_BLOB:
        return *this->data_string;
    case JKSN_ARRAY:
        {
            std::string res;
            bool first = true;
            for(const JKSNValue &i : *this->data_array) {
                if(!first)
                    res.append(1, ',');
                first = false;
                res.append(i.toString());
            }
            return res;
        }
    case JKSN_OBJECT:
        return "[object Object]";
    default:
        throw JKSNTypeError();
    }
}

bool JKSNValue::operator==(const JKSNValue &that) const {
    jksn_data_type this_type = this->getType();
    jksn_data_type that_type = that.getType();
    if(this_type == that_type || (this->isNumber() && that.isNumber()))
        switch(this_type) {
        case JKSN_BOOL:
            return this->toBool() == that.toBool();
        case JKSN_INT:
            switch(that_type) {
            case JKSN_INT:
                return this->toInt() == that.toInt();
            case JKSN_FLOAT:
                return this->toFloat() == that.toFloat();
            case JKSN_DOUBLE:
                return this->toDouble() == that.toDouble();
            case JKSN_LONG_DOUBLE:
                return this->toLongDouble() == that.toLongDouble();
            default:
                assert(that.isNumber());
                throw std::logic_error("unknown error occured during value comparision");
            }
        case JKSN_FLOAT:
            switch(that_type) {
            case JKSN_INT:
            case JKSN_FLOAT:
                return this->toFloat() == that.toFloat();
            case JKSN_DOUBLE:
                return this->toDouble() == that.toDouble();
            case JKSN_LONG_DOUBLE:
                return this->toLongDouble() == that.toLongDouble();
            default:
                assert(that.isNumber());
                throw std::logic_error("unknown error occured during value comparision");
            }
        case JKSN_DOUBLE:
            switch(that_type) {
            case JKSN_INT:
            case JKSN_FLOAT:
            case JKSN_DOUBLE:
                return this->toDouble() == that.toDouble();
            case JKSN_LONG_DOUBLE:
                return this->toLongDouble() == that.toLongDouble();
            default:
                assert(that.isNumber());
                throw std::logic_error("unknown error occured during value comparision");
            }
        case JKSN_LONG_DOUBLE:
            switch(that_type) {
            case JKSN_INT:
            case JKSN_FLOAT:
            case JKSN_DOUBLE:
            case JKSN_LONG_DOUBLE:
                return this->toLongDouble() == that.toLongDouble();
            default:
                assert(that.isNumber());
                throw std::logic_error("unknown error occured during value comparision");
            }
        case JKSN_STRING:
        case JKSN_BLOB:
            return this->toString() == that.toString();
        case JKSN_ARRAY:
            {
                const std::vector<JKSNValue> &this_vector = this->toVector();
                const std::vector<JKSNValue> &that_vector = that.toVector();
                if(this_vector.size() != that_vector.size())
                    return false;
                else {
                    for(auto this_iter = this_vector.cbegin(), that_iter = that_vector.cbegin();
                        this_iter != this_vector.end(); ++this_iter, ++that_iter)
                    if(*this_iter != *that_iter)
                        return false;
                    return true;
                }
            }
        case JKSN_OBJECT:
            {
                const std::map<JKSNValue, JKSNValue> &this_map = this->toMap();
                const std::map<JKSNValue, JKSNValue> &that_map = that.toMap();
                if(this_map.size() != that_map.size())
                    return false;
                else {
                    for(auto this_iter = this_map.cbegin(), that_iter = that_map.cbegin();
                        this_iter != this_map.end(); ++this_iter, ++that_iter)
                    if((*this_iter).first != (*that_iter).first || (*this_iter).second != (*that_iter).second)
                        return false;
                    return true;
                }
            }
        default:
            return true;
    } else
        return this_type == that_type;
}

bool JKSNValue::operator<(const JKSNValue &that) const {
    jksn_data_type this_type = this->getType();
    jksn_data_type that_type = that.getType();
    if(this_type == that_type || (this->isNumber() && that.isNumber()))
        switch(this_type) {
        case JKSN_BOOL:
            return this->toBool() < that.toBool();
        case JKSN_INT:
            switch(that_type) {
            case JKSN_INT:
                return this->toInt() < that.toInt();
            case JKSN_FLOAT:
                return this->toFloat() < that.toFloat();
            case JKSN_DOUBLE:
                return this->toDouble() < that.toDouble();
            case JKSN_LONG_DOUBLE:
                return this->toLongDouble() < that.toLongDouble();
            default:
                assert(that.isNumber());
                throw std::logic_error("unknown error occured during value comparision");
            }
        case JKSN_FLOAT:
            switch(that_type) {
            case JKSN_INT:
            case JKSN_FLOAT:
                return this->toFloat() < that.toFloat();
            case JKSN_DOUBLE:
                return this->toDouble() < that.toDouble();
            case JKSN_LONG_DOUBLE:
                return this->toLongDouble() < that.toLongDouble();
            default:
                assert(that.isNumber());
                throw std::logic_error("unknown error occured during value comparision");
            }
        case JKSN_DOUBLE:
            switch(that_type) {
            case JKSN_INT:
            case JKSN_FLOAT:
            case JKSN_DOUBLE:
                return this->toDouble() < that.toDouble();
            case JKSN_LONG_DOUBLE:
                return this->toLongDouble() < that.toLongDouble();
            default:
                assert(that.isNumber());
                throw std::logic_error("unknown error occured during value comparision");
            }
        case JKSN_LONG_DOUBLE:
            switch(that_type) {
            case JKSN_INT:
            case JKSN_FLOAT:
            case JKSN_DOUBLE:
            case JKSN_LONG_DOUBLE:
                return this->toLongDouble() < that.toLongDouble();
            default:
                assert(that.isNumber());
                throw std::logic_error("unknown error occured during value comparision");
            }
        case JKSN_STRING:
        case JKSN_BLOB:
            return this->toString() < that.toString();
        case JKSN_ARRAY:
            {
                const std::vector<JKSNValue> &this_vector = this->toVector();
                const std::vector<JKSNValue> &that_vector = that.toVector();
                auto this_iter = this_vector.cbegin();
                auto that_iter = that_vector.cbegin();
                for(; this_iter != this_vector.cend(); ++this_iter, ++that_iter) {
                    if(that_iter == that_vector.cend())
                        return false;
                    else if(*this_iter < *that_iter)
                        return true;
                    else if(*this_iter != *that_iter) /* > */
                        return false;
                }
                return that_iter != that_vector.cend();
            }
        case JKSN_OBJECT:
            {
                const std::map<JKSNValue, JKSNValue> &this_map = this->toMap();
                const std::map<JKSNValue, JKSNValue> &that_map = this->toMap();
                auto this_iter = this_map.cbegin();
                auto that_iter = that_map.cbegin();
                for(; this_iter != this_map.cend(); ++this_iter, ++that_iter) {
                    if(that_iter == that_map.cend())
                        return false;
                    else if((*this_iter).first < (*that_iter).first)
                        return true;
                    else if((*this_iter).first != (*that_iter).first) /* > */
                        return false;
                    else if((*this_iter).second < (*that_iter).second)
                        return true;
                    else if((*this_iter).second != (*that_iter).second)
                        return false;
                }
                return that_iter != that_map.cend();
            }
        default:
            return false;
        }
    else
        return this_type < that_type;
}

JKSNValue &JKSNValue::operator=(const JKSNValue &that) {
    if(this != &that) {
        union {
            std::string *new_string = nullptr;
            std::vector<JKSNValue> *new_array;
            std::map<JKSNValue, JKSNValue> *new_object;
        } new_data;
        switch(that.getType()) {
        case JKSN_BOOL:
            this->data_bool = that.toBool();
            break;
        case JKSN_INT:
            this->data_int = that.toInt();
            break;
        case JKSN_FLOAT:
            this->data_float = that.toFloat();
            break;
        case JKSN_DOUBLE:
            this->data_double = that.toDouble();
            break;
        case JKSN_LONG_DOUBLE:
            this->data_long_double = that.toLongDouble();
            break;
        case JKSN_STRING:
        case JKSN_BLOB:
            new_data.new_string = new std::string(that.toString());
            break;
        case JKSN_ARRAY:
            new_data.new_array = new std::vector<JKSNValue>(that.toVector());
            break;
        case JKSN_OBJECT:
            new_data.new_object = new std::map<JKSNValue, JKSNValue>(that.toMap());
            break;
        default:
            break;
        }
        this->~JKSNValue();
        switch((this->data_type = that.getType())) {
        case JKSN_STRING:
        case JKSN_BLOB:
            this->data_string = new_data.new_string;
            break;
        case JKSN_ARRAY:
            this->data_array = new_data.new_array;
            break;
        case JKSN_OBJECT:
            this->data_object = new_data.new_object;
            break;
        default:
            break;
        }
    }
    return *this;
}

JKSNValue &JKSNValue::operator=(JKSNValue &&that) {
    if(this != &that) {
        this->~JKSNValue();
        switch(that.getType()) {
        case JKSN_BOOL:
            this->data_bool = that.toBool();
            break;
        case JKSN_INT:
            this->data_int = that.toInt();
            break;
        case JKSN_FLOAT:
            this->data_float = that.toFloat();
            break;
        case JKSN_DOUBLE:
            this->data_double = that.toDouble();
            break;
        case JKSN_LONG_DOUBLE:
            this->data_long_double = that.toLongDouble();
            break;
        case JKSN_STRING:
        case JKSN_BLOB:
            this->data_string = that.data_string;
            break;
        case JKSN_ARRAY:
            this->data_array = that.data_array;
            break;
        case JKSN_OBJECT:
            this->data_object = that.data_object;
            break;
        default:
            break;
        }
        that.data_type = JKSN_UNDEFINED;
    }
    return *this;
}

}
