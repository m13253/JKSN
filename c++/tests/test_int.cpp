#include <iostream>
#include "jksn.hpp"

int main() {
    JKSN::JKSNValue value = 42;
    JKSN::dump(value, std::cout);
}