#include <iostream>
#include "jksn.hpp"

int main() {
	JKSN::JKSNValue value = JKSN::parse(std::cin);
	JKSN::dump(std::cout, value);
	return 0;
}