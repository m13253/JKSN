#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace JKSN {

class JKSNError : public std::exception {
public:
    JKSNError(const char *reason) noexcept;
    const char *what() const noexcept;
private:
    const char *reason;
};

class JKSNEncodeError : public JKSNError {};

class JKSNDecodeError : public JKSNError {};

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
    std::shared_ptr<std::vector<JKSNObject>> &toArray() const;
    std::shared_ptr<std::map<JKSNObject, JKSNObject>> &toObject() const;
    std::shared_ptr<JKSNObject> operator [](const JKSNObject &key) const;
private:
    jksn_data_type data_type;
    union {
        int data_bool;
        int64_t data_int;
        float data_float;
        double data_double;
        long double data_long_double;
        std::shared_ptr<std::string> data_string;
        std::shared_ptr<std::vector<JKSNObject>> data_array;
        std::shared_ptr<std::map<JKSNObject, JKSNObject>> data_object;
    };
};

class JKSNEncoder {
/* Note: With a certain JKSN encoder, the hashtable is preserved during each dump */
public:
    JKSNEncoder();
    ~JKSNEncoder();
    std::unique_ptr<std::string> dumpstr(const JKSNObject &obj, bool header = true, bool check_circular = true);
    std::unique_ptr<std::stringstream> dump(const JKSNObject &obj, bool header = true, bool check_circular = true);
private:
    struct JKSNEncoderImpl *impl;
};

class JKSNDecoder {
/* Note: With a certain JKSN decoder, the hashtable is preserved during each parse */
public:
    JKSNDecoder();
    ~JKSNDecoder();
    std::unique_ptr<JKSNObject> parsestr(const std::string &s, bool header = true);
    std::unique_ptr<JKSNObject> parse(const std::istream &fp, bool header = true);
private:
    struct JKSNDecoderImpl *impl;
};

std::unique_ptr<std::string> dumpstr(const JKSNObject &obj, bool header = true, bool check_circular = true);
std::unique_ptr<std::stringstream> dump(const JKSNObject &obj, bool header = true, bool check_circular = true);
std::unique_ptr<JKSNObject> parsestr(const std::string &s, bool header = true);
std::unique_ptr<JKSNObject> parse(const std::istream &fp, bool header = true);

}
