#include "../src/fs_shell.hpp"

#include <streambuf>
#include <iostream>
#include <fstream>
#include <string>


int main() {
    std::string str;
    while (str != "exit") {
        std::cout << "Name of file with scenario:";
        std::cin >> str;
        if (str == "exit") {
            break;
        }

        str = std::string("./scripts/").append(str);

        std::ifstream ifs{str, std::ios::in};
        if (!ifs.is_open()) {
            ifs = std::ifstream{str.append(".txt"), std::ios::in};

            if (!ifs.is_open()) {
                std::cout << "File with provided name does not exist\n\n";
                continue;
            }
        }
        shell::run(ifs, true);
        std::cout << std::endl;
    }

    return 0;
}
