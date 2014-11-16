#include <iostream>
#include "jksn.hpp"

int main() {
    JKSN::JKSNValue value = JKSN::JKSNValue::fromMap({
        {"key", "value"},
        {"键", "值"}
    });
    JKSN::dump(value, std::cout);
    return 0;
}