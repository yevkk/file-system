#pragma once

#include "fs.hpp"

#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <cstring>

class shell {
private:
    enum class commands {
        CREATE, DESTROY, OPEN, CLOSE, READ, WRITE, SEEK, DIR, INIT, SAVE, HELP, EXIT
    };

    static const std::map<std::string, std::pair<commands, std::uint8_t>> commands_map;

    static std::vector<std::string> parse_args(const std::string &args_string) {
        std::vector<std::string> args;

        std::istringstream isstream(args_string);
        std::string arg_word;
        while (isstream >> arg_word) {
            args.push_back(arg_word);
        }

        return args;
    };

public:
    shell() = delete;

    static void run() {
        lab_fs::file_system *fs;
        while (true) {
            std::string line;
            std::getline(std::cin, line);
            auto args = parse_args(line);

            if (args.empty() || !commands_map.contains(args[0])) {
                std::cout << "Error: wrong command, enter `help` to commands list\n";
                continue;
            }

            auto cmd_pair = commands_map.find(args[0])->second;

            if (cmd_pair.second != args.size()) {
                std::cout << "Error: wrong arguments number, enter `help` to commands list\n";
                continue;
            }

            switch (cmd_pair.first) {
                case commands::CREATE: {
                    std::cout << "a\n"; //todo: implement here
                    break;
                }
                case commands::DESTROY: {
                    std::cout << "b\n"; //todo: implement here
                    break;
                }
                case commands::OPEN: {
                    std::cout << "c\n"; //todo: implement here
                    break;
                }
                case commands::CLOSE: {
                    std::cout << "d\n"; //todo: implement here
                    break;
                }
                case commands::READ: {
                    std::cout << "e\n"; //todo: implement here
                    break;
                }
                case commands::WRITE: {
                    std::cout << "f\n"; //todo: implement here
                    break;
                }
                case commands::SEEK: {
                    std::cout << "g\n"; //todo: implement here
                    break;
                }
                case commands::DIR: {
                    std::cout << "h\n"; //todo: implement here
                    break;
                }
                case commands::INIT: {
                    std::cout << "i\n"; //todo: implement here
                    break;
                }
                case commands::SAVE: {
                    std::cout << "j\n"; //todo: implement here
                    break;
                }
                case commands::HELP: {
                    std::cout << "in <cyl_no> <surf_no> <sect_no> <sect_len> <disk_filename> - initialize file system\n";
                    std::cout << "sv <disk_filename> - save current file system\n";
                    std::cout << "cr - create file\n";              //todo: provide args description
                    std::cout << "de - destroy file\n";             //todo: provide args description
                    std::cout << "op - open file\n";                //todo: provide args description
                    std::cout << "cl - close file\n";               //todo: provide args description
                    std::cout << "rd - read from file\n";           //todo: provide args description
                    std::cout << "wr - write to file\n";            //todo: provide args description
                    std::cout << "sk - seek to position in file\n"; //todo: provide args description
                    std::cout << "dr - show directory content\n";
                    break;
                }
                case commands::EXIT: {
                    return;
                }
                default: {
                }
            }
        }
    }
};


const std::map<std::string, std::pair<shell::commands, std::uint8_t>> shell::commands_map = {
        {"cr",   {shell::commands::CREATE,  0}},  //todo: set required args number
        {"de",   {shell::commands::DESTROY, 0}},  //todo: set required args number
        {"op",   {shell::commands::OPEN,    0}},  //todo: set required args number
        {"cl",   {shell::commands::CLOSE,   0}},  //todo: set required args number
        {"rd",   {shell::commands::READ,    0}},  //todo: set required args number
        {"wr",   {shell::commands::WRITE,   0}},  //todo: set required args number
        {"sk",   {shell::commands::SEEK,    0}},  //todo: set required args number
        {"dr",   {shell::commands::DIR,     1}},
        {"in",   {shell::commands::INIT,    6}},
        {"sv",   {shell::commands::SAVE,    2}},
        {"help", {shell::commands::HELP,    1}},
        {"exit", {shell::commands::EXIT,    1}}
};