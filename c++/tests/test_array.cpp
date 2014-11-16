#include <iostream>
#include "jksn.hpp"

int main() {
    JKSN::JKSNValue value = {
        "element", "元素", "element", "元素"
    };
    JKSN::dump(value, std::cout);
    return 0;
}