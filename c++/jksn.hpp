#ifndef _JKSN_HPP_INCLUDED
#define _JKSN_HPP_INCLUDED

#include <cassert>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace JKSN {
    class JKSNError: public std::runtime_error {
    public:
        using runtime_error::runtime_error;
    };

    class JKSNEncodeError: public JKSNError {
    public:
        using JKSNError::JKSNError;
    };

    class JKSNDecodeError: public JKSNError {
    public:
        using JKSNError::JKSNError;
    };

    enum JKSNType {
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
    };
    
    class JKSNObject {
    public:
        class Undefined {};
        using Null = nullptr_t;    
        using Bool = bool;
        using Int64 = int64_t;
        using Float = float;
        using Double = double;
        using LongDouble = long double;
        using UTF8String = std::string;
        using Blob = std::string;
        using Array = std::vector<JKSNObject>;
        using Object = std::map<JKSNObject, JKSNObject>;
        class Unspecified {};
        
        JKSNObject(Undefined):           value_type{JKSN_UNDEFINED} {}
        JKSNObject(Null):                value_type{JKSN_NULL} {}
        JKSNObject(Bool b):              value_bool{b}, value_type{JKSN_BOOL} {}
        JKSNObject(Int64 i):             value_int{i}, value_type{JKSN_INT} {}
        JKSNObject(Float f):             value_float{f}, value_type{JKSN_FLOAT} {}
        JKSNObject(Double d):            value_double{d}, value_type{JKSN_DOUBLE} {}
        JKSNObject(LongDouble l):        value_long_double{l}, value_type{JKSN_LONG_DOUBLE} {}
        JKSNObject(const UTF8String& s): value_pstring{new auto{s}}, value_type{JKSN_STRING} {}
        JKSNObject(UTF8String&& s):      value_pstring{new auto{s}}, value_type{JKSN_STRING} {}
        JKSNObject(const Blob& b):       value_pblob{new auto{b}}, value_type{JKSN_BLOB} {}
        JKSNObject(Blob&& b):            value_pblob{new auto{b}}, value_type{JKSN_BLOB} {}
        JKSNObject(const Array& a):      value_parray{new auto{a}}, value_type{JKSN_ARRAY} {}
        JKSNObject(Array&& a):           value_parray{new auto{a}}, value_type{JKSN_ARRAY} {}
        JKSNObject(const Object& o):     value_pobject{new auto{o}}, value_type{JKSN_OBJECT} {}
        JKSNObject(Object&& o):          value_pobject{new auto{o}}, value_type{JKSN_OBJECT} {}
        JKSNObject(Unspecified):         value_type{JKSN_UNSPECIFIED} {}
        ~JKSNObject();
        JKSNObject& operator = (const JKSNObject&);

        JKSNType type() const { return value_type; }
        Bool toBool() const;
        Int64 toInt() const;
        Float toFloat() const;
        Double toDouble() const;
        LongDouble toLongDouble() const;
        UTF8String toString() const;
        Blob toBlob() const;
        Array toArray() const;
        Object toObject() const;

        bool operator == (const JKSNObject&) const;
        bool operator < (const JKSNObject&) const;

        JKSNObject& operator [] (const JKSNObject& key) const {
            if (value_type == JKSN_OBJECT || key.value_type != JKSN_INT)
                return (*value_pobject)[key];
            else if (value_type == JKSN_ARRAY || key.value_type == JKSN_INT)
                return (*value_parray)[key.toInt()];
            else
                assert(false);
        }
        JKSNObject& operator [] (size_t index) {
            assert(value_type == JKSN_ARRAY);
            return (*value_array)[index];
        }

    private:
        JKSNType value_type;
        union {
            Bool value_bool = false;
            Int64 value_int;
            Float value_float;
            Double value_double;
            LongDouble value_long_double;
            std::shared_ptr<UTF8String> value_pstring;
            std::shared_ptr<Blob> value_pblob;
            std::shared_ptr<Array> value_parray;
            std::shared_ptr<Object> value_pobject;
        };
    };
}

#endif /* _JKSN_HPP_INCLUDED */

// class JKSNEncoder {
// // Note: With a certain JKSN encoder, the hashtable is preserved during each dump
// public:
//     JKSNEncoder();
//     ~JKSNEncoder();
//     std::unique_ptr<std::string> dumpstr(const JKSNObject &obj, bool header = true, bool check_circular = true);
//     std::unique_ptr<std::stringstream> dump(const JKSNObject &obj, bool header = true, bool check_circular = true);
// private:
//     struct JKSNEncoderImpl *impl;
// };
// 
// class JKSNDecoder {
// // Note: With a certain JKSN decoder, the hashtable is preserved during each parse
// public:
//     JKSNDecoder();
//     ~JKSNDecoder();
//     std::unique_ptr<JKSNObject> parsestr(const std::string &s, bool header = true);
//     std::unique_ptr<JKSNObject> parse(std::istream &fp, bool header = true);
// private:
//     struct JKSNDecoderImpl *impl;
// };
// 
// std::unique_ptr<std::string> dumpstr(const JKSNObject &obj, bool header = true, bool check_circular = true);
// std::unique_ptr<std::stringstream> dump(const JKSNObject &obj, bool header = true, bool check_circular = true);
// std::unique_ptr<JKSNObject> parsestr(const std::string &s, bool header = true);
// std::unique_ptr<JKSNObject> parse(std::istream &fp, bool header = true);
// 
// }
