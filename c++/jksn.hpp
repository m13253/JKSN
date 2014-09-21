#include <stdint.h>
#include <exception>
#include <map>
#include <string>
#include <vector>

namespace JKSN {

class JKSNError: public std::exception {
public:
    JKSNError(const char *reason) throw();
    const char *what() const throw();
private:
    const char *reason;
};

class JKSNEncodeError: public JKSNError {
};

class JKSNDecodeError: public JKSNError {
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

class JKSNUnspecified{
};

class JKSNObject {
public:
    JKSNObject();
    JKSNObject(bool data);
    JKSNObject(int64_t data);
    JKSNObject(float data);
    JKSNObject(double data);
    JKSNObject(long double data);
    JKSNObject(std::string &data, bool isblob);
    JKSNObject(std::vector<JKSNObject> &data);
    JKSNObject(std::map<JKSNObject, JKSNObject> &data);
    JKSNObject(JKSNUnspecified &data);
    jksn_data_type getType() const;
    bool toBool() const;
    int64_t toInt64() const;
    float toFloat() const;
    double toDouble() const;
    long double toLongDouble() const;
    std::string &toString() const;
    std::string &toBlob() const;
    std::vector<JKSNObject> &toArray() const;
    std::map<JKSNObject, JKSNObject> &toObject() const;
    JKSNObject &operator [](JKSNObject &key) const;
};

class JKSNEncoder {
public:
    JKSNEncoder();
    ~JKSNEncoder();
    std::string &dumps(const JKSNObject &obj, bool header = true, bool check_circular = true);
    std::stringstream &dump(const JKSNObject &obj, bool header = true, bool check_circular = true);
};

class JKSNDecoder {
public:
    JKSNDecoder();
    ~JKSNDecoder();
    JKSNObject &loads(const std::string &s, bool header = true);
    JKSNObject &load(const std::istream &fp, bool header = true);
};

std::string &dumps(const JKSNObject &obj, bool header = true, bool check_circular = true);
std::stringstream &dump(const JKSNObject &obj, bool header = true, bool check_circular = true);
JKSNObject &loads(const std::string &s, bool header = true);
JKSNObject &load(const std::istream &fp, bool header = true);

};
