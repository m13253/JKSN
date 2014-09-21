#include "jksn.hpp"

namespace JKSN {

struct JKSNObjectImpl {
};

struct JKSNEncoderImpl {
};

struct JKSNDecoderImpl {
};

JKSNError::JKSNError(const char *reason) throw() {
    this->reason = reason;
}

const char *JKSNError::what() const throw() {
    return this->reason;
}

void JKSNObject::init() {
    this->impl = new struct JKSNObjectImpl;
}

JKSNObject::JKSNObject() {
    this->init();
}

JKSNObject::~JKSNObject() {
    delete this->impl;
}

};
