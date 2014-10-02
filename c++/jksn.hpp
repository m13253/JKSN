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

#ifndef _JKSN_HPP_INCLUDED
#define _JKSN_HPP_INCLUDED

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
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
    
    class JKSNTypeError: public JKSNError {
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
    
    class JKSNValue {
    public:
        class Hasher {
        public:
            size_t operator () (const JKSNValue& j) const {
                return j.hashCode();
            }
        };
        
        class Undefined {};
        class Null {};
        using Array = std::vector<JKSNValue>;
        using Object = std::unordered_map<JKSNValue, JKSNValue, Hasher>;
        class Unspecified {};
        
        JKSNValue():                    value_type{JKSN_UNDEFINED} {}
        JKSNValue(Undefined):           value_type{JKSN_UNDEFINED} {}
        JKSNValue(Null):                value_type{JKSN_NULL} {}
        JKSNValue(bool b):              value_type{JKSN_BOOL}, value_bool{b} {}
        JKSNValue(int i):               value_type{JKSN_INT}, value_int{i} {}
        JKSNValue(int64_t i):           value_type{JKSN_INT}, value_int{i} {}
        JKSNValue(float f):             value_type{JKSN_FLOAT}, value_float{f} {}
        JKSNValue(double d):            value_type{JKSN_DOUBLE}, value_double{d} {}
        JKSNValue(long double l):       value_type{JKSN_LONG_DOUBLE}, value_long_double{l} {}
        JKSNValue(const std::string& s, bool is_blob = false):
                value_type{is_blob ? JKSN_BLOB : JKSN_STRING},
                value_pstr{new auto(s)} {}
        JKSNValue(std::string&& s, bool is_blob = false):
                value_type{is_blob ? JKSN_BLOB : JKSN_STRING},
                value_pstr{new auto(s)} {}
        JKSNValue(const char* s, bool is_blob = false):
                value_type{is_blob ? JKSN_BLOB : JKSN_STRING},
                value_pstr{new std::string{s}} {}
        JKSNValue(const Array& a):      value_type{JKSN_ARRAY}, value_parray{new auto(a)} {}
        JKSNValue(Array&& a):           value_type{JKSN_ARRAY}, value_parray{new auto(a)} {}
        JKSNValue(const Object& o):     value_type{JKSN_OBJECT}, value_pobject{new auto(o)} {}
        JKSNValue(Object&& o):          value_type{JKSN_OBJECT}, value_pobject{new auto(o)} {}
        JKSNValue(Unspecified):         value_type{JKSN_UNSPECIFIED} {}
        
        JKSNValue(const JKSNValue&);
        JKSNValue(JKSNValue&&);
        JKSNValue& operator = (const JKSNValue&);
        JKSNValue& operator = (JKSNValue&&);
        ~JKSNValue();

        bool toBool() const;
        int64_t toInt() const;
        float toFloat() const;
        double toDouble() const;
        long double toLongDouble() const;
        std::string toString() const;
        std::string toBlob() const;
        Array toArray() const;
        Object toObject() const;

        bool isUndefined() const   { return value_type == JKSN_UNDEFINED; }
        bool isNull() const        { return value_type == JKSN_NULL; }
        bool isBool() const        { return value_type == JKSN_BOOL; }
        bool isInt() const         { return value_type == JKSN_INT; }
        bool isFloat() const       { return value_type == JKSN_FLOAT; }
        bool isDouble() const      { return value_type == JKSN_DOUBLE; }
        bool isLongDouble() const  { return value_type == JKSN_LONG_DOUBLE; }
        bool isString() const      { return value_type == JKSN_STRING; }
        bool isBlob() const        { return value_type == JKSN_BLOB; }
        bool isArray() const       { return value_type == JKSN_ARRAY; }
        bool isObject() const      { return value_type == JKSN_OBJECT; }
        bool isUnspecified() const { return value_type == JKSN_UNSPECIFIED; }

        bool operator == (const JKSNValue&) const;
        size_t hashCode() const;

        JKSNValue& operator [] (const JKSNValue& key) const {
            if (value_type == JKSN_OBJECT)
                return (*value_pobject)[key];
            else
                throw JKSNTypeError{"Operator [] on non-object type."};
        }
        JKSNValue& operator [] (size_t index) {
            if (value_type == JKSN_ARRAY)
                return (*value_parray)[index];
            else
                throw JKSNTypeError{"Operator [] on non-array type."};
        }

    private:
        JKSNType value_type;
        union {
            bool value_bool = false;
            int64_t value_int;
            float value_float;
            double value_double;
            long double value_long_double;
            std::shared_ptr<std::string> value_pstr;
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
//     std::unique_ptr<std::string> dumpstr(const JKSNValue &obj, bool header = true, bool check_circular = true);
//     std::unique_ptr<std::stringstream> dump(const JKSNValue &obj, bool header = true, bool check_circular = true);
// private:
//     struct JKSNEncoderImpl *impl;
// };
// 
// class JKSNDecoder {
// // Note: With a certain JKSN decoder, the hashtable is preserved during each parse
// public:
//     JKSNDecoder();
//     ~JKSNDecoder();
//     std::unique_ptr<JKSNValue> parsestr(const std::string &s, bool header = true);
//     std::unique_ptr<JKSNValue> parse(std::istream &fp, bool header = true);
// private:
//     struct JKSNDecoderImpl *impl;
// };
// 
// std::unique_ptr<std::string> dumpstr(const JKSNValue &obj, bool header = true, bool check_circular = true);
// std::unique_ptr<std::stringstream> dump(const JKSNValue &obj, bool header = true, bool check_circular = true);
// std::unique_ptr<JKSNValue> parsestr(const std::string &s, bool header = true);
// std::unique_ptr<JKSNValue> parse(std::istream &fp, bool header = true);
// 
// }
