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
        CREATE = 1, DESTROY, OPEN, CLOSE, READ, WRITE, SEEK, DIR, INIT, SAVE, HELP, EXIT
    };

    static const std::map<std::string, commands> commands_map;

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
                std::cout << "Error, enter `help` to commands list\n";
                continue;
            }
            
            switch (commands_map.find(args[0])->second) {
                case commands::CREATE: {
                    std::cout << "a\n";
                    break;
                }
                case commands::DESTROY: {
                    std::cout << "b\n";
                    break;
                }
                case commands::OPEN: {
                    std::cout << "c\n";
                    break;
                }
                case commands::CLOSE: {
                    std::cout << "d\n";
                    break;
                }
                case commands::READ: {
                    std::cout << "e\n";
                    break;
                }
                case commands::WRITE: {
                    std::cout << "f\n";
                    break;
                }
                case commands::SEEK: {
                    std::cout << "g\n";
                    break;
                }
                case commands::DIR: {
                    std::cout << "h\n";
                    break;
                }
                case commands::INIT: {
                    std::cout << "i\n";
                    break;
                }
                case commands::SAVE: {
                    std::cout << "j\n";
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


const std::map<std::string, shell::commands> shell::commands_map = {
        {"cr",   shell::commands::CREATE},
        {"de",   shell::commands::DESTROY},
        {"op",   shell::commands::OPEN},
        {"cl",   shell::commands::CLOSE},
        {"rd",   shell::commands::READ},
        {"wr",   shell::commands::WRITE},
        {"sk",   shell::commands::SEEK},
        {"dr",   shell::commands::DIR},
        {"in",   shell::commands::INIT},
        {"sv",   shell::commands::SAVE},
        {"help", shell::commands::HELP},
        {"exit", shell::commands::EXIT}
};