#include <iostream>
#include "jksn.hpp"

int main() {
    JKSN::JKSNValue value = {
        JKSN::JKSNValue::fromMap({
            {"name", "Jason"},
            {"email", "jason@example.com"},
            {"phone", "777-777-7777"}
        }),
        JKSN::JKSNValue::fromMap({
            {"name", "Jackson"},
            {"age", 17},
            {"email", "jackson@example.com"},
            {"phone", "888-888-8888"}
        }),
    };
    JKSN::dump(value, std::cout);
    return 0;
}