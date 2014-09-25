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
    namespace impl {
        class any {
        public:
            virtual ~any() = default;
        };
        
        template <class T>
        class any_value: public any {
        public:
            any(const T& v): value{v} {}
            any(T&& v): value{std::move(v)} {}
            ~any_value() = default;
            T value;
        };
        
        using any_ptr = std::shared_ptr<any>;
        
        template <class T>
        inline any_ptr to_any(T&& x) {
            using U = std::remove_cv_t<std::remove_reference_t<T>>;
            return std::make_shared<any_value<U>>(std::forward<T>(x));
        }
        
        class bad_any_cast {};
        
        template <class T>
        inline T& any_cast(const any_ptr& a) {
            if (auto* p = dynamic_cast<any_value<T>*>(a.get()))
                return p->value;
            else
                throw bad_any_cast{};
        }
        
        template <class T>
        inline T& any_to(const any_ptr& a) {
            return static_cast<any_value<T>*>(a.get())->value;
        }
        
        template <class T>
        inline bool any_is(const any_ptr& a) {
            return dynamic_cast<any_value<T>*>(a.get()) != nullptr;
        }
    }

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
        
        JKSNObject(Undefined):           value{}, value_type{JKSN_UNDEFINED} {}
        JKSNObject(Null):                value{}, value_type{JKSN_NULL} {}
        JKSNObject(Bool b):              value{to_any(b)}, value_type{JKSN_BOOL} {}
        JKSNObject(Int64 i):             value{to_any(i)}, value_type{JKSN_INT} {}
        JKSNObject(Float f):             value{to_any(f)}, value_type{JKSN_FLOAT} {}
        JKSNObject(Double d):            value{to_any(d)}, value_type{JKSN_DOUBLE} {}
        JKSNObject(LongDouble l):        value{to_any(l)}, value_type{JKSN_LONG_DOUBLE} {}
        JKSNObject(const UTF8String& s): value{to_any(s)}, value_type{JKSN_STRING} {}
        JKSNObject(UTF8String&& s):      value{to_any(s)}, value_type{JKSN_STRING} {}
        JKSNObject(const Blob& b):       value{to_any(b)}, value_type{JKSN_BLOB} {}
        JKSNObject(Blob&& b):            value{to_any(b)}, value_type{JKSN_BLOB} {}
        JKSNObject(const Array& a):      value{to_any(a)}, value_type{JKSN_ARRAY} {}
        JKSNObject(Array&& a):           value{to_any(a)}, value_type{JKSN_ARRAY} {}
        JKSNObject(const Object& o):     value{to_any(o)}, value_type{JKSN_OBJECT} {}
        JKSNObject(Object&& o):          value{to_any(o)}, value_type{JKSN_OBJECT} {}
        JKSNObject(Unspecified):         value{}, value_type{JKSN_UNSPECIFIED} {}

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

        bool operator == (const JKSNObject& that) const;
        bool operator < (const JKSNObject& that) const;

        JKSNObject& operator [] (const JKSNObject& key) const {
            if (value_type == JKSN_OBJECT || key.value_type != JKSN_INT)
                return impl::any_to<Object>(value)[key];
            else if (value_type == JKSN_ARRAY || key.value_type == JKSN_INT)
                return impl::any_to<Array>(value)[key.toInt()];
            else
                assert(false);
        }
        JKSNObject& operator [] (size_t index) {
            assert(value_type == JKSN_ARRAY);
            return impl::any_to<Array>(value)[index];
        }

    private:
        impl::any_ptr value;
        JKSNType value_type;
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
