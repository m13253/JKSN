#include <iostream>
#include <sstream>
#include <string>
#include "../jksn.cpp"

bool to_utf8 = false;

int main(int argc, char *argv[]) {
    for(int argi = 1; argi < argc; ++argi)
        if(argv[argi][0] == '-' && argv[argi][1] == 'd' && argv[argi][2] == '\0') {
            to_utf8 = true;
            break;
        }
    if(to_utf8) {
        std::u16string utf16str;
        char buf[2];
        while(std::cin.read(buf, 2))
            utf16str.push_back(char16_t(uint8_t(buf[0]) | uint16_t(uint8_t(buf[1])) << 8));
        std::cerr << "Input UTF-16 size: " << utf16str.size() << std::endl;
        std::cout << JKSN::UTF16ToUTF8(utf16str);
    } else {
        std::stringstream sstr;
        while(sstr << std::cin.rdbuf()) {
        }
        const std::string &utf8str = sstr.str();
        std::cerr << "Input UTF-8 size: " << utf8str.size() << std::endl;
        std::cout << JKSN::UTF8ToUTF16LE(utf8str);
    }
    return 0;
}