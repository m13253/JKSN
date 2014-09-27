#ifndef _JKSN_HPP_INCLUDED
#define _JKSN_HPP_INCLUDED

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
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
    
    class JKSNObject {
    public:
        class Hasher {
        public:
            size_t operator () (const JKSNObject& j) const {
                return j.hashCode();
            }
        };
        class Undefined {};
        class Null {};
        using Array = std::vector<JKSNObject>;
        using Object = std::unordered_map<JKSNObject, JKSNObject, Hasher>;
        class Unspecified {};
        
        JKSNObject():                    value_type{JKSN_UNDEFINED} {}
        JKSNObject(Undefined):           value_type{JKSN_UNDEFINED} {}
        JKSNObject(Null):                value_type{JKSN_NULL} {}
        JKSNObject(bool b):              value_type{JKSN_BOOL}, value_bool{b} {}
        JKSNObject(int i):               value_type{JKSN_INT}, value_int{i} {}
        JKSNObject(int64_t i):           value_type{JKSN_INT}, value_int{i} {}
        JKSNObject(float f):             value_type{JKSN_FLOAT}, value_float{f} {}
        JKSNObject(double d):            value_type{JKSN_DOUBLE}, value_double{d} {}
        JKSNObject(long double l):       value_type{JKSN_LONG_DOUBLE}, value_long_double{l} {}
        JKSNObject(const std::string& s, bool is_blob = false):
                value_type{is_blob ? JKSN_BLOB : JKSN_STRING},
                value_pstr{new auto(s)} {}
        JKSNObject(std::string&& s, bool is_blob = false):
                value_type{is_blob ? JKSN_BLOB : JKSN_STRING},
                value_pstr{new auto(s)} {}
        JKSNObject(const char* s, bool is_blob = false):
                value_type{is_blob ? JKSN_BLOB : JKSN_STRING},
                value_pstr{new std::string{s}} {}
        JKSNObject(const Array& a):      value_type{JKSN_ARRAY}, value_parray{new auto(a)} {}
        JKSNObject(Array&& a):           value_type{JKSN_ARRAY}, value_parray{new auto(a)} {}
        JKSNObject(const Object& o):     value_type{JKSN_OBJECT}, value_pobject{new auto(o)} {}
        JKSNObject(Object&& o):          value_type{JKSN_OBJECT}, value_pobject{new auto(o)} {}
        JKSNObject(Unspecified):         value_type{JKSN_UNSPECIFIED} {}
        JKSNObject(const JKSNObject&);
        JKSNObject(JKSNObject&&);
        JKSNObject& operator = (const JKSNObject&);
        JKSNObject& operator = (JKSNObject&&);
        ~JKSNObject();

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

        bool operator == (const JKSNObject&) const;
        size_t hashCode() const;

        JKSNObject& operator [] (const JKSNObject& key) const {
            if (value_type == JKSN_OBJECT || key.value_type != JKSN_INT)
                return (*value_pobject)[key];
            else if (value_type == JKSN_ARRAY || key.value_type == JKSN_INT)
                return (*value_parray)[key.toInt()];
            else
                throw JKSNTypeError{"Operator [] on non-aggregation type."};
        }
        JKSNObject& operator [] (size_t index) {
            if (value_type != JKSN_ARRAY)
                throw JKSNTypeError{"Operator [] on non-array type."};
            return (*value_parray)[index];
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
