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
        class Null {};
        using Array = std::vector<JKSNObject>;
        using Object = std::map<JKSNObject, JKSNObject>;
        class Unspecified {};
        
        JKSNObject(Undefined):           value_type{JKSN_UNDEFINED} {}
        JKSNObject(Null):                value_type{JKSN_NULL} {}
        JKSNObject(bool b):              value_bool{b}, value_type{JKSN_BOOL} {}
        JKSNObject(int64_t i):           value_int{i}, value_type{JKSN_INT} {}
        JKSNObject(float f):             value_float{f}, value_type{JKSN_FLOAT} {}
        JKSNObject(double d):            value_double{d}, value_type{JKSN_DOUBLE} {}
        JKSNObject(long double l):       value_long_double{l}, value_type{JKSN_LONG_DOUBLE} {}
        JKSNObject(const std::string& s, bool is_blob = false):
                value_pstr{new auto{s}},
                value_type{is_blob ? JKSN_BLOB : JKSN_STRING} {}
        JKSNObject(std::string&& s, bool is_blob = false):
                value_pstr{new auto{s}},
                value_type{is_blob ? JKSN_BLOB : JKSN_STRING} {}
        JKSNObject(const Array& a):      value_parray{new auto{a}}, value_type{JKSN_ARRAY} {}
        JKSNObject(Array&& a):           value_parray{new auto{a}}, value_type{JKSN_ARRAY} {}
        JKSNObject(const Object& o):     value_pobject{new auto{o}}, value_type{JKSN_OBJECT} {}
        JKSNObject(Object&& o):          value_pobject{new auto{o}}, value_type{JKSN_OBJECT} {}
        JKSNObject(Unspecified):         value_type{JKSN_UNSPECIFIED} {}
        ~JKSNObject();
        JKSNObject& operator = (const JKSNObject&);

        JKSNType type() const { return value_type; }
        bool toBool() const;
        int64_t toInt() const;
        float toFloat() const;
        double toDouble() const;
        long double toLongDouble() const;
        std::string toString() const;
        std::string toBlob() const;
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
