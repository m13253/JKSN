#include <iostream>
#include "jksn.hpp"

int main() {
    JKSN::JKSNValue value = JKSN::parse(std::cin);
    JKSN::dump(value, std::cout);
    return 0;
}