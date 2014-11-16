#include <iostream>
#include "jksn.hpp"

int main() {
    JKSN::JKSNValue value = {
        100, 101, 99, 130, 1000
    };
    JKSN::dump(value, std::cout);
    return 0;
}