#include <iostream>
#include "jksn.hpp"

int main() {
	JKSN::JKSNValue value = 4.2e10;
	JKSN::dump(value, std::cout);
}