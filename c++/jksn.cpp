#include "jksn.hpp"

namespace JKSN {

JKSNError::JKSNError(const char *reason) throw() {
    this->reason = reason;
}

const char *JKSNError::what() const throw() {
    return this->reason;
}

};
