#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
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
            ~any_value() = default;
            T value;
        };
        
        using any_ptr = std::shared_ptr<any>;
        
        template <class T>
        any_ptr to_any(const T& x) {
            return std::make_shared<any<T>>(x);
        }
        
        class bad_any_cast {};
        
        template <class T>
        T& any_to(any_ptr a) {
            if (auto* p = dynamic_cast<any_value<T>*>(a.get()))
                return p->value;
            else
                throw bad_any_cast{};
        }
        
        template <class T>
        bool any_is(any_ptr a) {
            return dynamic_cast<any_value<T>*>(a.get()) != nullptr;
        }
    }

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
    } JKSNType;
    
    using Null = nullptr_t;    
    using Bool = bool;
    using Int8 = int8_t;
    using Int16 = int16_t;
    using Int32 = int32_t;
    using Float = float;
    using Double = double;
    using LongDouble = long double;
    using UTF8String = std::u8string;
    using UTF16String = std::u16string;
    using BlobString = std::vector<Int8>;
    using Array = std::vector<JKSNObject>;
    using Object = std::map<JKSNObject, JKSNObject>;
    class Undefined {};
    class Unspecified {};
    
    class JKSNObject {
    public:
        
    };

/*
class JKSNError : public std::runtime_error {
public:
    using runtime_error::runtime_error;
};

class JKSNEncodeError : public JKSNError {
    using JKSNError::JKSNError;
};

class JKSNDecodeError : public JKSNError {
    using JKSNError::JKSNError;
};


class JKSNUnspecified {};

class JKSNObject {
public:
    JKSNObject();
    JKSNObject(bool data);
    JKSNObject(int64_t data);
    JKSNObject(float data);
    JKSNObject(double data);
    JKSNObject(long double data);
    JKSNObject(std::shared_ptr<std::string> data, bool isblob = false);
    JKSNObject(std::shared_ptr<std::vector<JKSNObject>> data);
    JKSNObject(std::shared_ptr<std::map<JKSNObject, JKSNObject>> data);
    JKSNObject(JKSNUnspecified &data);
    ~JKSNObject();

    jksn_data_type getType() const;
    bool toBool() const;
    int64_t toInt() const;
    float toFloat() const;
    double toDouble() const;
    long double toLongDouble() const;
    std::shared_ptr<std::string> toString() const;
    std::shared_ptr<std::string> toBlob() const;
    std::shared_ptr<std::vector<JKSNObject>> toArray() const;
    std::shared_ptr<std::map<JKSNObject, JKSNObject>> toObject() const;

    bool operator==(const JKSNObject &that) const;
    bool operator<(const JKSNObject &that) const;
    std::shared_ptr<JKSNObject> operator[](const JKSNObject &key) const;
    std::shared_ptr<JKSNObject> operator[](size_t key) const;
    std::shared_ptr<JKSNObject> operator[](const std::string &key) const;

private:
    jksn_data_type data_type;
    union {
        int data_bool;
        int64_t data_int;
        float data_float;
        double data_double;
        long double data_long_double;
        std::shared_ptr<std::string> *data_string;
        std::shared_ptr<std::vector<JKSNObject>> *data_array;
        std::shared_ptr<std::map<JKSNObject, JKSNObject>> *data_object;
    };
};

class JKSNEncoder {
// Note: With a certain JKSN encoder, the hashtable is preserved during each dump
public:
    JKSNEncoder();
    ~JKSNEncoder();
    std::unique_ptr<std::string> dumpstr(const JKSNObject &obj, bool header = true, bool check_circular = true);
    std::unique_ptr<std::stringstream> dump(const JKSNObject &obj, bool header = true, bool check_circular = true);
private:
    struct JKSNEncoderImpl *impl;
};

class JKSNDecoder {
// Note: With a certain JKSN decoder, the hashtable is preserved during each parse
public:
    JKSNDecoder();
    ~JKSNDecoder();
    std::unique_ptr<JKSNObject> parsestr(const std::string &s, bool header = true);
    std::unique_ptr<JKSNObject> parse(std::istream &fp, bool header = true);
private:
    struct JKSNDecoderImpl *impl;
};

std::unique_ptr<std::string> dumpstr(const JKSNObject &obj, bool header = true, bool check_circular = true);
std::unique_ptr<std::stringstream> dump(const JKSNObject &obj, bool header = true, bool check_circular = true);
std::unique_ptr<JKSNObject> parsestr(const std::string &s, bool header = true);
std::unique_ptr<JKSNObject> parse(std::istream &fp, bool header = true);

}
*/