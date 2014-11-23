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
#include <cstdlib>
#include <cstring>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
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
        if(!stream.put(char(this->control)))
            return stream;
        if(!(stream << this->data))
            return stream;
        if(!(stream << this->buf))
            return stream;
        if(recursive)
            for(const JKSNProxy &i : this->children)
                if(!i.output(stream))
                    return stream;
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
    std::list<JKSNProxy> children;
    uint8_t hash = 0;
};

class JKSNCache {
public:
    bool haslastint = false;
    intmax_t lastint;
    std::array<std::shared_ptr<std::string>, 256> texthash = {{nullptr}};
    std::array<std::shared_ptr<std::string>, 256> blobhash = {{nullptr}};
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
    static JKSNProxy dumpArray(const std::vector<const JKSNValue *> &obj, const JKSNValue *origin = nullptr);
    static bool testSwapAvailability(const std::vector<const JKSNValue *> &obj);
    static JKSNProxy encodeStraightArray(const std::vector<const JKSNValue *> &obj, const JKSNValue *origin = nullptr);
    static JKSNProxy encodeSwappedArray(const std::vector<const JKSNValue *> &obj, const JKSNValue *origin = nullptr);
    static JKSNProxy dumpObject(const JKSNValue &obj);
    static JKSNProxy dumpUnspecified(const JKSNValue &obj);
    JKSNProxy &optimize(JKSNProxy &obj);
};

class JKSNDecoderPrivate {
public:
    JKSNValue parseValue(std::istream &fp);
private:
    JKSNCache cache;
    static uintmax_t decodeInt(std::istream &fp, size_t size);
    static JKSNValue parseFloat(std::istream &fp);
    static JKSNValue parseDouble(std::istream &fp);
    static JKSNValue parseLongDouble(std::istream &fp);
    JKSNValue parseSwappedArray(std::istream &fp, size_t column_length);
};

static std::string UTF8ToUTF16LE(const std::string &utf8str, bool strict = false);
static std::string UTF16ToUTF8(const std::u16string &utf16str);
static uint8_t DJBHash(const std::string &obj, uint8_t iv = 0);
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

std::ostream &JKSNEncoder::dump(const JKSNValue &obj, std::ostream &result, bool header) {
    JKSNProxy proxy = this->p->dumpToProxy(obj);
    if(header && !result.write("jk!", 3))
        return result;
    proxy.output(result);
    return result;
}

std::string JKSNEncoder::dump(const JKSNValue &obj, bool header) {
    std::ostringstream result;
    if(!this->dump(obj, result, header))
        throw JKSNEncodeError("no enough memory");
    return result.str();
}

JKSNProxy JKSNEncoderPrivate::dumpToProxy(const JKSNValue &obj) {
    JKSNProxy proxy = this->dumpValue(obj);
    this->optimize(proxy);
    return proxy;
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

JKSNProxy JKSNEncoderPrivate::dumpString(const JKSNValue &obj) {
    std::string obj_short = obj.toString();
    bool is_utf16 = false;
    try {
        std::string obj_utf16 = UTF8ToUTF16LE(obj_short, true);
        if(obj_utf16.size() < obj_short.size()) {
            obj_short = std::move(obj_utf16);
            is_utf16 = true;
        }
    } catch(JKSNTypeError) {
    }
    uint8_t control = is_utf16 ? 0x30 : 0x40;
    uintmax_t length = is_utf16 ? obj_short.size()/2 : obj_short.size();
    std::unique_ptr<JKSNProxy> result;
    if(length <= (is_utf16 ? 0xb : 0xc))
        result.reset(new JKSNProxy(&obj, control | uint8_t(length), std::string(), std::move(obj_short)));
    else if(length <= 0xff)
        result.reset(new JKSNProxy(&obj, control | 0xe, encodeInt(length, 1), std::move(obj_short)));
    else if(length <= 0xffff)
        result.reset(new JKSNProxy(&obj, control | 0xd, encodeInt(length, 2), std::move(obj_short)));
    else
        result.reset(new JKSNProxy(&obj, control | 0xf, encodeInt(length, 0), std::move(obj_short)));
    result->hash = DJBHash(result->buf);
    return std::move(*result);
}

JKSNProxy JKSNEncoderPrivate::dumpBlob(const JKSNValue &obj) {
    std::string blob = obj.toBlob();
    size_t length = blob.size();
    std::unique_ptr<JKSNProxy> result;
    if(length <= 0xb)
        result.reset(new JKSNProxy(&obj, 0x50 | uint8_t(length), std::string(), std::move(blob)));
    else if(length <= 0xff)
        result.reset(new JKSNProxy(&obj, 0x5e, encodeInt(length, 1), std::move(blob)));
    else if(length <= 0xffff)
        result.reset(new JKSNProxy(&obj, 0x5d, encodeInt(length, 2), std::move(blob)));
    else
        result.reset(new JKSNProxy(&obj, 0x5f, encodeInt(length, 0), std::move(blob)));
    result->hash = DJBHash(result->buf);
    return std::move(*result);
}

bool JKSNEncoderPrivate::testSwapAvailability(const std::vector<const JKSNValue *> &obj) {
    bool columns = false;
    for(const JKSNValue *const row : obj)
        if(!row->isObject())
            return false;
        else
            columns = columns || !row->toMap().empty();
    return columns;
}

JKSNProxy JKSNEncoderPrivate::encodeStraightArray(const std::vector<const JKSNValue *> &obj, const JKSNValue *origin) {
    size_t length = obj.size();
    std::unique_ptr<JKSNProxy> result;
    if(length <= 0xc)
        result.reset(new JKSNProxy(origin, 0x80 | uint8_t(length)));
    else if(length <= 0xff)
        result.reset(new JKSNProxy(origin, 0x8e, encodeInt(length, 1)));
    else if(length <= 0xffff)
        result.reset(new JKSNProxy(origin, 0x8d, encodeInt(length, 2)));
    else
        result.reset(new JKSNProxy(origin, 0x8f, encodeInt(length, 0)));
    for(const JKSNValue *const i : obj)
        result->children.push_back(dumpValue(*i));
    assert(result->children.size() == length);
    return std::move(*result);
}

JKSNProxy JKSNEncoderPrivate::encodeSwappedArray(const std::vector<const JKSNValue *> &obj, const JKSNValue *origin) {
    std::list<JKSNValue> columns;
    std::unordered_set<JKSNValue> columns_set;
    for(const JKSNValue *const row : obj)
        for(const std::pair<JKSNValue, JKSNValue> &column : row->toMap())
            if(columns_set.find(column.first) == columns_set.end()) {
                columns.push_back(column.first);
                columns_set.insert(column.first);
            }
    size_t collen = columns.size();
    std::unique_ptr<JKSNProxy> result;
    if(collen <= 0xc)
        result.reset(new JKSNProxy(origin, 0xa0 | uint8_t(collen)));
    else if(collen <= 0xff)
        result.reset(new JKSNProxy(origin, 0xae, encodeInt(collen, 1)));
    else if(collen <= 0xffff)
        result.reset(new JKSNProxy(origin, 0xad, encodeInt(collen, 2)));
    else
        result.reset(new JKSNProxy(origin, 0xaf, encodeInt(collen, 0)));
    for(const JKSNValue &column : columns) {
        result->children.push_back(dumpValue(column));
        std::vector<const JKSNValue *> columns_value;
        columns_value.reserve(obj.size());
        for(const JKSNValue *const row : obj) {
            static JKSNValue unspecified_value = JKSNValue::fromUnspecified();
            std::map<JKSNValue, JKSNValue>::const_iterator it = row->toMap().find(column);
            columns_value.push_back(it != row->toMap().end() ? &it->second : &unspecified_value);
        }
        result->children.push_back(dumpArray(columns_value));
    }
    assert(result->children.size() == collen*2);
    return std::move(*result);
}

JKSNProxy JKSNEncoderPrivate::dumpArray(const JKSNValue &obj) {
    std::vector<const JKSNValue *> obj_vector;
    obj_vector.reserve(obj.toVector().size());
    for(const JKSNValue &i : obj.toVector())
        obj_vector.push_back(&i);
    return dumpArray(obj_vector, &obj);
}

JKSNProxy JKSNEncoderPrivate::dumpArray(const std::vector<const JKSNValue *> &obj, const JKSNValue *origin) {
    JKSNProxy result = encodeStraightArray(obj, origin);
    if(testSwapAvailability(obj)) {
        JKSNProxy result_swapped = encodeSwappedArray(obj);
        if(result_swapped.size(3) < result.size(3))
            result = std::move(result_swapped);
    }
    return result;
}

JKSNProxy JKSNEncoderPrivate::dumpObject(const JKSNValue &obj) {
    size_t length = obj.toMap().size();
    std::unique_ptr<JKSNProxy> result;
    if(length <= 0xc)
        result.reset(new JKSNProxy(&obj, 0x90 | uint8_t(length)));
    else if(length <= 0xff)
        result.reset(new JKSNProxy(&obj, 0x9e, encodeInt(length, 1)));
    else if(length <= 0xffff)
        result.reset(new JKSNProxy(&obj, 0x9d, encodeInt(length, 2)));
    else
        result.reset(new JKSNProxy(&obj, 0x9f, encodeInt(length, 0)));
    for(const std::pair<JKSNValue, JKSNValue> &item : obj.toMap()) {
        result->children.push_back(dumpValue(item.first));
        result->children.push_back(dumpValue(item.second));
    }
    assert(result->children.size() == length*2);
    return std::move(*result);
}

JKSNProxy JKSNEncoderPrivate::dumpUnspecified(const JKSNValue &obj) {
    return JKSNProxy(&obj, 0xa0);
}

JKSNProxy &JKSNEncoderPrivate::optimize(JKSNProxy &obj) {
    uint8_t control = obj.control & 0xf0;
    switch(control) {
        case 0x10:
            if (this->cache.haslastint) {
                intmax_t delta = obj.origin->toInt() - this->cache.lastint;
                if(std::abs(delta) < std::abs(obj.origin->toInt())) {
                    uint8_t new_control;
                    std::string new_data;
                    if(delta >= 0 && delta <= 0x5)
                        new_control = 0xb0 | uint8_t(delta);
                    else if(delta >= -0x5 && delta <= -0x1)
                        new_control = 0xb0 | uint8_t(delta+11);
                    else if(delta >= -0x80 && delta <= 0x7f) {
                        new_control = 0xbd;
                        new_data = encodeInt(uintmax_t(delta), 1);
                    } else if(delta >= -0x8000 && delta <= 0x7fff) {
                        new_control = 0xbc;
                        new_data = encodeInt(uintmax_t(delta), 2);
                    } else if((delta >= -0x80000000 && delta <= -0x200000) ||
                              (delta >= 0x200000 && delta <= 0x7fffffff)) {
                        new_control = 0xbb;
                        new_data = encodeInt(uintmax_t(delta), 4);
                    } else if(delta >= 0) {
                        new_control = 0xbf;
                        new_data = encodeInt(uintmax_t(delta), 0);
                    } else {
                        new_control = 0xbe;
                        new_data = encodeInt(uintmax_t(-delta), 0);
                    }
                    if(new_data.size() < obj.data.size()) {
                        obj.control = new_control;
                        obj.data = std::move(new_data);
                    }
                }
            }
            this->cache.haslastint = true;
            this->cache.lastint = obj.origin->toInt();
            break;
        case 0x30:
        case 0x40:
            if(obj.buf.size() > 1) {
                if(this->cache.texthash[obj.hash] && *this->cache.texthash[obj.hash] == obj.buf) {
                    obj.control = 0x3c;
                    obj.data = encodeInt(obj.hash, 1);
                    obj.buf.clear();
                } else
                    this->cache.texthash[obj.hash] = std::make_shared<std::string>(obj.buf);
            }
            break;
        case 0x50:
            if(obj.buf.size() > 1) {
                if(this->cache.blobhash[obj.hash] && *this->cache.blobhash[obj.hash] == obj.buf) {
                    obj.control = 0x3c;
                    obj.data = encodeInt(obj.hash, 1);
                    obj.buf.clear();
                } else
                    this->cache.blobhash[obj.hash] = std::make_shared<std::string>(obj.buf);
            }
            break;
        default:
            for(JKSNProxy &child : obj.children)
                this->optimize(child);
    }
    return obj;
}

std::string JKSNEncoderPrivate::encodeInt(uintmax_t number, size_t size) {
    switch(size) {
    case 1:
        return std::string({
            char(uint8_t(number))
        });
    case 2:
        return std::string({
            char(uint8_t(number >> 8)),
            char(uint8_t(number))
        });
    case 4:
        return std::string({
            char(uint8_t(number >> 24)),
            char(uint8_t(number >> 16)),
            char(uint8_t(number >> 8)),
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
    if(header) {
        char header_buf[3];
        if(!fp.read(header_buf, 3) || fp.gcount() != 3 || std::memcmp(header_buf, "jk!", 3))
            fp.seekg(-fp.gcount(), fp.cur);
    }
    return this->p->parseValue(fp);
}

JKSNValue JKSNDecoder::parse(const std::string &str, bool header) {
    std::istringstream stream(str);
    return this->parse(stream, header);
}

JKSNValue JKSNDecoderPrivate::parseValue(std::istream &fp) {
    for(;;) {
        char signed_control;
        if(!fp.get(signed_control))
            throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
        uint8_t control = uint8_t(signed_control);
        uint8_t ctrlhi = control & 0xf0;
        switch(ctrlhi) {
        /* Special values */
        case 0x00:
            switch(control) {
            case 0x00:
                return JKSNValue();
            case 0x01:
                return JKSNValue(nullptr);
            case 0x02:
                return JKSNValue(false);
            case 0x03:
                return JKSNValue(true);
            case 0x0f:
                throw JKSNDecodeError("this JKSN decoder does not support JSON literals");
            }
            break;
        /* Integers */
        case 0x10:
            this->cache.haslastint = true;
            switch(control) {
            case 0x1b:
                this->cache.lastint = intmax_t(int32_t(this->decodeInt(fp, 4)));
                break;
            case 0x1c:
                this->cache.lastint = intmax_t(int16_t(this->decodeInt(fp, 2)));
                break;
            case 0x1d:
                this->cache.lastint = intmax_t(int8_t(this->decodeInt(fp, 1)));
                break;
            case 0x1e:
                this->cache.lastint = -intmax_t(this->decodeInt(fp, 0));
                if(this->cache.lastint >= 0)
                    throw JKSNDecodeError("this build of JKSN decoder does not support variable length integers");
                break;
            case 0x1f:
                this->cache.lastint = intmax_t(this->decodeInt(fp, 0));
                if(this->cache.lastint < 0)
                    throw JKSNDecodeError("this build of JKSN decoder does not support variable length integers");
                break;
            default:
                this->cache.lastint = control & 0xf;
            }
            return JKSNValue(this->cache.lastint);
        /* Floating point numbers */
        case 0x20:
            switch(control) {
            case 0x20:
                return JKSNValue(NAN);
            case 0x2b:
                return this->parseLongDouble(fp);
            case 0x2c:
                return this->parseDouble(fp);
            case 0x2d:
                return this->parseFloat(fp);
            case 0x2e:
                return JKSNValue(-INFINITY);
            case 0x2f:
                return JKSNValue(INFINITY);
            }
            break;
        /* UTF-16 strings */
        case 0x30:
            {
                size_t strsize;
                switch(control) {
                case 0x3c:
                    {
                        char hashvalue;
                        if(!fp.get(hashvalue))
                            throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
                        if(this->cache.texthash[uint8_t(hashvalue)])
                            return JKSNValue(*this->cache.texthash[uint8_t(hashvalue)]);
                        else
                            throw JKSNDecodeError("JKSN stream requires a non-existing hash");
                    }
                case 0x3d:
                    strsize = this->decodeInt(fp, 2);
                    break;
                case 0x3e:
                    strsize = this->decodeInt(fp, 1);
                    break;
                case 0x3f:
                    strsize = this->decodeInt(fp, 0);
                    break;
                default:
                    strsize = control & 0xf;
                }
                std::vector<char16_t> strbuf(strsize);
                if(!fp.read(reinterpret_cast<char *>(strbuf.data()), std::streamsize(strsize*2)))
                    throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
                if(!isLittleEndian())
                    for(char16_t &i : strbuf)
                        i = char16_t(uint16_t(i) >> 8 | uint16_t(i) << 8);
                std::string result = UTF16ToUTF8(std::u16string(strbuf.cbegin(), strbuf.cend()));
                this->cache.texthash[DJBHash(result)].reset(new std::string(result));
                return JKSNValue(std::move(result));
            }
        /* UTF-8 strings */
        case 0x40:
            {
                size_t strsize;
                switch(control) {
                case 0x4d:
                    strsize = this->decodeInt(fp, 2);
                    break;
                case 0x4e:
                    strsize = this->decodeInt(fp, 1);
                    break;
                case 0x3f:
                    strsize = this->decodeInt(fp, 0);
                    break;
                default:
                    strsize = control & 0xf;
                }
                std::vector<char> strbuf(strsize);
                if(!fp.read(strbuf.data(), std::streamsize(strsize)))
                    throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
                std::string result = std::string(strbuf.cbegin(), strbuf.cend());
                this->cache.texthash[DJBHash(result)].reset(new std::string(result));
                return JKSNValue(std::move(result));
            }
        /* Blob strings */
        case 0x50:
            {
                size_t strsize;
                switch(control) {
                case 0x5c:
                    {
                        char hashvalue;
                        if(!fp.get(hashvalue))
                            throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
                        if(this->cache.blobhash[uint8_t(hashvalue)])
                            return JKSNValue(*this->cache.blobhash[uint8_t(hashvalue)]);
                        else
                            throw JKSNDecodeError("JKSN stream requires a non-existing hash");
                    }
                case 0x5d:
                    strsize = this->decodeInt(fp, 2);
                    break;
                case 0x5e:
                    strsize = this->decodeInt(fp, 1);
                    break;
                case 0x5f:
                    strsize = this->decodeInt(fp, 0);
                    break;
                default:
                    strsize = control & 0xf;
                }
                std::vector<char> strbuf(strsize);
                if(!fp.read(strbuf.data(), std::streamsize(strsize)))
                    throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
                std::string result = std::string(strbuf.cbegin(), strbuf.cend());
                this->cache.blobhash[DJBHash(result)].reset(new std::string(result));
                return JKSNValue(std::move(result), true);
            }
        /* Hashtable refreshers */
        case 0x70:
            {
                size_t objlen;
                switch(control) {
                case 0x70:
                    this->cache.texthash.fill(nullptr);
                    this->cache.texthash.fill(nullptr);
                    continue;
                case 0x7d:
                    objlen = this->decodeInt(fp, 2);
                    break;
                case 0x7e:
                    objlen = this->decodeInt(fp, 1);
                    break;
                case 0x7f:
                    objlen = this->decodeInt(fp, 0);
                    break;
                default:
                    objlen = control & 0xf;
                }
                while(objlen--)
                    this->parseValue(fp);
                continue;
            }
        /* Arrays */
        case 0x80:
            {
                size_t objlen;
                switch(control) {
                case 0x8d:
                    objlen = this->decodeInt(fp, 2);
                    break;
                case 0x8e:
                    objlen = this->decodeInt(fp, 1);
                    break;
                case 0x8f:
                    objlen = this->decodeInt(fp, 0);
                    break;
                default:
                    objlen = control & 0xf;
                }
                std::vector<JKSNValue> result;
                result.reserve(objlen);
                while(objlen--)
                    result.push_back(this->parseValue(fp));
                return JKSNValue(std::move(result));
            }
        /* Objects */
        case 0x90:
            {
                size_t objlen;
                switch(control) {
                case 0x9d:
                    objlen = this->decodeInt(fp, 2);
                    break;
                case 0x9e:
                    objlen = this->decodeInt(fp, 1);
                    break;
                case 0x9f:
                    objlen = this->decodeInt(fp, 0);
                    break;
                default:
                    objlen = control & 0xf;
                }
                std::map<JKSNValue, JKSNValue> result;
                while(objlen--) {
                    JKSNValue key = this->parseValue(fp);
                    result[std::move(key)] = this->parseValue(fp);
                }
                return JKSNValue(std::move(result));
            }
        /* Row-col swapped arrays */
        case 0xa0:
            {
                size_t collen;
                switch(control) {
                case 0xa0:
                    return JKSNValue::fromUnspecified();
                case 0xad:
                    collen = this->decodeInt(fp, 2);
                    break;
                case 0xae:
                    collen = this->decodeInt(fp, 1);
                    break;
                case 0xaf:
                    collen = this->decodeInt(fp, 0);
                    break;
                default:
                    collen = control & 0xf;
                }
                return this->parseSwappedArray(fp, collen);
            }
        /* Delta encoded integers */
        case 0xb0:
            {
                intmax_t delta;
                switch(control) {
                case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5:
                    delta = control & 0xf;
                    break;
                case 0xb6: case 0xb7: case 0xb8: case 0xb9: case 0xba:
                    delta = intmax_t(control & 0xf)-11;
                    break;
                case 0xbb:
                    delta = intmax_t(int32_t(this->decodeInt(fp, 4)));
                    break;
                case 0xbc:
                    delta = intmax_t(int16_t(this->decodeInt(fp, 2)));
                    break;
                case 0xbd:
                    delta = intmax_t(int8_t(this->decodeInt(fp, 1)));
                    break;
                case 0xbe:
                    delta = intmax_t(-this->decodeInt(fp, 0));
                    if(delta >= 0)
                        throw JKSNDecodeError("this build of JKSN decoder does not support variable length integers");
                    break;
                case 0xbf:
                    delta = intmax_t(this->decodeInt(fp, 0));
                    if(delta < 0)
                        throw JKSNDecodeError("this build of JKSN decoder does not support variable length integers");
                    break;
                }
                if(!this->cache.haslastint)
                    throw JKSNDecodeError("JKSN stream contains an invalid delta encoded integer");
                this->cache.haslastint = true;
                this->cache.lastint += delta;
                return JKSNValue(this->cache.lastint);
            }
        /* Lengthless arrays */
        case 0xc0:
            switch(control) {
                case 0xc8:
                    {
                        std::vector<JKSNValue> result;
                        for(;;) {
                            JKSNValue item = this->parseValue(fp);
                            if(!item.isUnspecified())
                                result.push_back(std::move(item));
                            else
                                return JKSNValue(std::move(result));
                        }
                    }
            }
        case 0xf0:
            /* Ignore checksums */
            if(control <= 0xf5) {
                std::vector<char> buf;
                switch(control) {
                case 0xf0:
                    buf.reserve(1);
                    if(!fp.read(buf.data(), 1))
                        throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
                    continue;
                case 0xf1:
                    buf.reserve(4);
                    if(!fp.read(buf.data(), 4))
                        throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
                    continue;
                case 0xf2:
                    buf.reserve(16);
                    if(!fp.read(buf.data(), 16))
                        throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
                    continue;
                case 0xf3:
                    buf.reserve(20);
                    if(!fp.read(buf.data(), 20))
                        throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
                    continue;
                case 0xf4:
                    buf.reserve(32);
                    if(!fp.read(buf.data(), 32))
                        throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
                    continue;
                case 0xf5:
                    buf.reserve(64);
                    if(!fp.read(buf.data(), 64))
                        throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
                    continue;
                }
            } else if (control >= 0xf8 && control <= 0xfd) {
                std::vector<char> buf;
                JKSNValue result;
                switch(control) {
                case 0xf8:
                    result = parseValue(fp);
                    buf.reserve(1);
                    if(!fp.read(buf.data(), 1))
                        throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
                    continue;
                case 0xf9:
                    result = parseValue(fp);
                    buf.reserve(4);
                    if(!fp.read(buf.data(), 4))
                        throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
                    continue;
                case 0xfa:
                    result = parseValue(fp);
                    buf.reserve(16);
                    if(!fp.read(buf.data(), 16))
                        throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
                    continue;
                case 0xfb:
                    result = parseValue(fp);
                    buf.reserve(20);
                    if(!fp.read(buf.data(), 20))
                        throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
                    continue;
                case 0xfc:
                    result = parseValue(fp);
                    buf.reserve(32);
                    if(!fp.read(buf.data(), 32))
                        throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
                    continue;
                case 0xfd:
                    result = parseValue(fp);
                    buf.reserve(64);
                    if(!fp.read(buf.data(), 64))
                        throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
                    continue;
                }
                return result;
            /* Ignore pragmas */
            } else if(control == 0xff) {
                parseValue(fp);
                continue;
            }
        }
        throw JKSNDecodeError("cannot encode unrecognizable type of value");
    }
}

uintmax_t JKSNDecoderPrivate::decodeInt(std::istream &fp, size_t size) {
    switch(size) {
    case 1:
        {
            char buffer;
            if(!fp.get(buffer))
                throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
            return uintmax_t(uint8_t(buffer));
        }
    case 2:
        {
            char buffer[2];
            if(!fp.read(buffer, 2))
                throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
            return uintmax_t(uint8_t(buffer[0])) << 8 |
                   uintmax_t(uint8_t(buffer[1]));
        }
    case 4:
        {
            char buffer[4];
            if(!fp.read(buffer, 4))
                throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
            return uintmax_t(uint8_t(buffer[0])) << 24 |
                   uintmax_t(uint8_t(buffer[1])) << 16 |
                   uintmax_t(uint8_t(buffer[2])) << 8 |
                   uintmax_t(uint8_t(buffer[3]));
        }
    case 0:
        {
            char thisbyte;
            uintmax_t result = 0;
            do {
                if(result & ~(~ uintmax_t(0) >> 7))
                    throw JKSNDecodeError("this build of JKSN decoder does not support variable length integers");
                if(!fp.get(thisbyte))
                    throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
                result = (result << 7) | (uint8_t(thisbyte) & 0x7f);
            } while(uint8_t(thisbyte) & 0x80);
            return result;
        }
    default:
        assert(size == 1 || size == 2 || size == 4 || size == 0);
        abort();
    }
}

JKSNValue JKSNDecoderPrivate::parseFloat(std::istream &fp) {
    static_assert(sizeof (float) == 4, "sizeof (float) should be 4");
    char buffer[4];
    if(!fp.read(buffer, 4))
        throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
    const union {
        uint32_t data_int;
        float data_float;
    } conv = {
        uint32_t(uint8_t(buffer[0])) << 24 |
        uint32_t(uint8_t(buffer[1])) << 16 |
        uint32_t(uint8_t(buffer[2])) << 8 |
        uint32_t(uint8_t(buffer[3]))
    };
    return JKSNValue(conv.data_float);
}

JKSNValue JKSNDecoderPrivate::parseDouble(std::istream &fp) {
    static_assert(sizeof (double) == 8, "sizeof (double) should be 8");
    char buffer[8];
    if(!fp.read(buffer, 8))
        throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
    const union {
        uint64_t data_int;
        double data_double;
    } conv = {
        uint64_t(uint8_t(buffer[0])) << 56 |
        uint64_t(uint8_t(buffer[1])) << 48 |
        uint64_t(uint8_t(buffer[2])) << 40 |
        uint64_t(uint8_t(buffer[3])) << 32 |
        uint64_t(uint8_t(buffer[4])) << 24 |
        uint64_t(uint8_t(buffer[5])) << 16 |
        uint64_t(uint8_t(buffer[6])) << 8 |
        uint64_t(uint8_t(buffer[7]))
    };
    return JKSNValue(conv.data_double);
}

JKSNValue JKSNDecoderPrivate::parseLongDouble(std::istream &fp) {
    if(sizeof (long double) == 12) {
        char buffer[10];
        if(!fp.read(buffer, 10))
            throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
        union {
            uint8_t data_int[12];
            long double data_long_double;
        } conv;
        if(isLittleEndian()) {
            conv.data_int[0] = uint8_t(buffer[9]);
            conv.data_int[1] = uint8_t(buffer[8]);
            conv.data_int[2] = uint8_t(buffer[7]);
            conv.data_int[3] = uint8_t(buffer[6]);
            conv.data_int[4] = uint8_t(buffer[5]);
            conv.data_int[5] = uint8_t(buffer[4]);
            conv.data_int[6] = uint8_t(buffer[3]);
            conv.data_int[7] = uint8_t(buffer[2]);
            conv.data_int[8] = uint8_t(buffer[1]);
            conv.data_int[9] = uint8_t(buffer[0]);
            conv.data_int[10] = 0;
            conv.data_int[11] = 0;
        } else {
            conv.data_int[0] = 0;
            conv.data_int[1] = 0;
            conv.data_int[2] = uint8_t(buffer[0]);
            conv.data_int[3] = uint8_t(buffer[1]);
            conv.data_int[4] = uint8_t(buffer[2]);
            conv.data_int[5] = uint8_t(buffer[3]);
            conv.data_int[6] = uint8_t(buffer[4]);
            conv.data_int[7] = uint8_t(buffer[5]);
            conv.data_int[8] = uint8_t(buffer[6]);
            conv.data_int[9] = uint8_t(buffer[7]);
            conv.data_int[10] = uint8_t(buffer[8]);
            conv.data_int[11] = uint8_t(buffer[9]);
        }
        return JKSNValue(conv.data_long_double);
    } else if(sizeof (long double) == 16) {
        char buffer[10];
        if(!fp.read(buffer, 10))
            throw JKSNDecodeError("JKSN stream may be truncated or corrupted");
        union {
            uint8_t data_int[16];
            long double data_long_double;
        } conv;
        if(isLittleEndian()) {
            conv.data_int[0] = uint8_t(buffer[9]);
            conv.data_int[1] = uint8_t(buffer[8]);
            conv.data_int[2] = uint8_t(buffer[7]);
            conv.data_int[3] = uint8_t(buffer[6]);
            conv.data_int[4] = uint8_t(buffer[5]);
            conv.data_int[5] = uint8_t(buffer[4]);
            conv.data_int[6] = uint8_t(buffer[3]);
            conv.data_int[7] = uint8_t(buffer[2]);
            conv.data_int[8] = uint8_t(buffer[1]);
            conv.data_int[9] = uint8_t(buffer[0]);
            conv.data_int[10] = 0;
            conv.data_int[11] = 0;
            conv.data_int[12] = 0;
            conv.data_int[13] = 0;
            conv.data_int[14] = 0;
            conv.data_int[15] = 0;
        } else {
            conv.data_int[0] = 0;
            conv.data_int[1] = 0;
            conv.data_int[2] = 0;
            conv.data_int[3] = 0;
            conv.data_int[4] = 0;
            conv.data_int[5] = 0;
            conv.data_int[6] = uint8_t(buffer[0]);
            conv.data_int[7] = uint8_t(buffer[1]);
            conv.data_int[8] = uint8_t(buffer[2]);
            conv.data_int[9] = uint8_t(buffer[3]);
            conv.data_int[10] = uint8_t(buffer[4]);
            conv.data_int[11] = uint8_t(buffer[5]);
            conv.data_int[12] = uint8_t(buffer[6]);
            conv.data_int[13] = uint8_t(buffer[7]);
            conv.data_int[14] = uint8_t(buffer[8]);
            conv.data_int[15] = uint8_t(buffer[9]);
        }
        return JKSNValue(conv.data_long_double);
    } else
        throw JKSNEncodeError("this build of JKSN decoder does not support long double numbers");
}

JKSNValue JKSNDecoderPrivate::parseSwappedArray(std::istream &fp, size_t column_length) {
    std::vector<JKSNValue> result;
    while(column_length--) {
        JKSNValue column_name = this->parseValue(fp);
        JKSNValue column_values = this->parseValue(fp);
        if(!column_values.isArray())
            throw JKSNDecodeError("JKSN row-col swapped array requires an array but not found");
        std::vector<JKSNValue> &column_values_vector = column_values.toVector();
        for(size_t i = 0; i < column_values_vector.size(); ++i) {
            if(i == result.size())
                result.push_back(JKSNValue::fromMap(std::map<JKSNValue, JKSNValue>()));
            if(!column_values_vector[i].isUnspecified())
                result[i].toMap()[column_name] = std::move(column_values_vector[i]);
        }
    }
    return JKSNValue(std::move(result));
}


static inline bool isLittleEndian() {
    static const union {
        uint16_t word;
        uint8_t byte;
    } endiantest = {1};
    return endiantest.byte == 1;
}

static bool UTF8CheckContinuation(const std::string &utf8str, size_t start, size_t check_length) {
    if(utf8str.size() > start + check_length) {
        while(check_length--)
            if((uint8_t(utf8str[++start]) & 0xc0) != 0x80)
                return false;
        return true;
    } else
        return false;
}

static std::string UTF8ToUTF16LE(const std::string &utf8str, bool strict) {
    std::string utf16str;
    size_t i = 0;
    utf16str.reserve(utf8str.size());
    while(i < utf8str.size()) {
        if(uint8_t(utf8str[i]) < 0x80) {
            utf16str.append({utf8str[i], '\0'});
            ++i;
            continue;
        } else if(uint8_t(utf8str[i]) < 0xc0) {
        } else if(uint8_t(utf8str[i]) < 0xe0) {
            if(UTF8CheckContinuation(utf8str, i, 1)) {
                uint32_t ucs4 = uint32_t(utf8str[i] & 0x1f) << 6 | uint32_t(utf8str[i+1] & 0x3f);
                if(ucs4 >= 0x80) {
                    utf16str.append({
                        char(ucs4),
                        char(ucs4 >> 8)
                    });
                    i += 2;
                    continue;
                }
            }
        } else if(uint8_t(utf8str[i]) < 0xf0) {
            if(UTF8CheckContinuation(utf8str, i, 2)) {
                uint32_t ucs4 = uint32_t(utf8str[i] & 0xf) << 12 | uint32_t(utf8str[i+1] & 0x3f) << 6 | (utf8str[i+2] & 0x3f);
                if(ucs4 >= 0x800 && (ucs4 & 0xf800) != 0xd800) {
                    utf16str.append({
                        char(ucs4),
                        char(ucs4 >> 8)
                    });
                    i += 3;
                    continue;
                }
            }
        } else if(uint8_t(utf8str[i]) < 0xf8) {
            if(UTF8CheckContinuation(utf8str, i, 3)) {
                uint32_t ucs4 = uint32_t(utf8str[i] & 0x7) << 18 | uint32_t(utf8str[i+1] & 0x3f) << 12 | uint32_t(utf8str[i+2] & 0x3f) << 6 | uint32_t(utf8str[i+3] & 0x3f);
                if(ucs4 >= 0x10000 && ucs4 < 0x110000) {
                    ucs4 -= 0x10000;
                    utf16str.append({
                        char(ucs4 >> 10),
                        char((ucs4 >> 18) | 0xd8),
                        char(ucs4),
                        char(((ucs4 >> 8) & 0x3) | 0xdc)
                    });
                    i += 4;
                    continue;
                }
            }
        }
        if(strict)
            throw JKSNTypeError();
        else {
            utf16str.append("\xfd\xff", 2);
            ++i;
        }
    }
    utf16str.shrink_to_fit();
    return utf16str;
}

static std::string UTF16ToUTF8(const std::u16string &utf16str) {
    std::string utf8str;
    size_t i = 0;
    utf8str.reserve(utf16str.size()*2);
    while(i < utf16str.size()) {
        if(uint16_t(utf16str[i]) < 0x80) {
            utf8str.push_back(char(utf16str[i]));
            ++i;
        } else if(uint16_t(utf16str[i]) < 0x800) {
            utf8str.append({
                char(uint16_t(utf16str[i]) >> 6 | 0xc0),
                char((uint16_t(utf16str[i]) & 0x3f) | 0x80)
            });
            ++i;
        } else if((uint16_t(utf16str[i]) & 0xf800) != 0xd800) {
                utf8str.append({
                    char(uint16_t(utf16str[i]) >> 12 | 0xe0),
                    char(((uint16_t(utf16str[i]) >> 6) & 0x3f) | 0x80),
                    char((uint16_t(utf16str[i]) & 0x3f) | 0x80)
                });
                ++i;
        } else if(i+1 < utf16str.size() && uint32_t(utf16str[i] & 0xfc00) == 0xd800 && uint32_t(utf16str[i+1] & 0xfc00) == 0xdc00) {
            uint32_t ucs4 = uint32_t((utf16str[i] & 0x3ff) << 10 | (utf16str[i+1] & 0x3ff)) + 0x10000;
            utf8str.append({
                char(ucs4 >> 18 | 0xf0),
                char(((ucs4 >> 12) & 0x3f) | 0x80),
                char(((ucs4 >> 6) & 0x3f) | 0x80),
                char((ucs4 & 0x3f) | 0x80),
            });
            i += 2;
        } else {
            utf8str.append("\xef\xbf\xbd", 3);
            ++i;
        }
    }
    utf8str.shrink_to_fit();
    return utf8str;
}

static uint8_t DJBHash(const std::string &buf, uint8_t iv) {
    unsigned int result = iv;
    for(char i : buf)
        result += (result << 5) + uint8_t(i);
    return result;
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
        this->data_type = that.data_type;
        that.data_type = JKSN_UNDEFINED;
    }
    return *this;
}

}
