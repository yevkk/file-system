#pragma once

#include "fs.hpp"

#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <cstring>

class shell {
private:
    class command {
    public:
        enum class actions {
            CREATE, DESTROY, OPEN, CLOSE, READ, WRITE, SEEK, DIR, INIT, SAVE, HELP, EXIT
        };

        command(actions action, unsigned args_min_no, unsigned args_max_no) :
            action{action},
            args_min_no{args_min_no},
            args_max_no{args_max_no} {};

        command(actions action, unsigned args_min_no) :
                action{action},
                args_min_no{args_min_no},
                args_max_no{args_min_no} {};

        actions action;
        unsigned args_min_no;
        unsigned args_max_no;
    };

    static const std::map<std::string, const command> commands_map;

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
        lab_fs::file_system *fs = nullptr;
        while (true) {
            std::string line;
            std::getline(std::cin, line);
            auto args = parse_args(line);

            if (args.empty() || !commands_map.contains(args[0])) {
                std::cout << "error: wrong command, enter `help` to commands list\n";
                continue;
            }

            auto cmd = commands_map.find(args[0])->second;

            if ((args.size() - 1 < cmd.args_min_no) || (args.size() - 1 > cmd.args_max_no)) {
                std::cout << "error: wrong arguments number, enter `help` to commands list\n";
                continue;
            }

            if (fs == nullptr && !(cmd.action == command::actions::INIT ||
                                   cmd.action == command::actions::HELP ||
                                   cmd.action == command::actions::EXIT)) {
                std::cout << "error: file system is not initialized\n";
                continue;
            }

            switch (cmd.action) {
                case command::actions::CREATE: {
                    std::cout << "a\n"; //todo: implement here
                    break;
                }
                case command::actions::DESTROY: {
                    std::cout << "b\n"; //todo: implement here
                    break;
                }
                case command::actions::OPEN: {
                    std::cout << "c\n"; //todo: implement here
                    break;
                }
                case command::actions::CLOSE: {
                    std::cout << "d\n"; //todo: implement here
                    break;
                }
                case command::actions::READ: {
                    std::cout << "e\n"; //todo: implement here
                    break;
                }
                case command::actions::WRITE: {
                    std::cout << "f\n"; //todo: implement here
                    break;
                }
                case command::actions::SEEK: {
                    std::cout << "g\n"; //todo: implement here
                    break;
                }
                case command::actions::DIR: {
                    std::cout << "h\n"; //todo: implement here
                    break;
                }
                case command::actions::INIT: {
                    if (fs != nullptr) {
                        std::cout << "error: file system is already loaded; save current file system to create/restore another one";
                        break;
                    }
                    auto res = lab_fs::file_system::init(std::stoull(args[1]),
                                                         std::stoull(args[2]),
                                                         std::stoull(args[3]),
                                                         std::stoull(args[4]),
                                                         args[5]);
                    fs = res.first;
                    switch (res.second) {
                        case lab_fs::CREATED:
                            std::cout << "disk initialized\n";
                            break;
                        case lab_fs::RESTORED:
                            std::cout << "disk restored\n";
                            break;
                        default:
                            break;
                    }
                    break;
                }
                case command::actions::SAVE: {
                    fs->save();
                    std::cout << "disk saved\n";
                    delete fs;
                    fs = nullptr;
                    break;
                }
                case command::actions::HELP: {
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
                case command::actions::EXIT: {
                    return;
                }
                default: {
                }
            }
        }
    }
};


const std::map<std::string, const shell::command> shell::commands_map = {
        {"cr",   shell::command{shell::command::actions::CREATE,  0}},  //todo: set required args number
        {"de",   shell::command{shell::command::actions::DESTROY, 0}},  //todo: set required args number
        {"op",   shell::command{shell::command::actions::OPEN,    0}},  //todo: set required args number
        {"cl",   shell::command{shell::command::actions::CLOSE,   0}},  //todo: set required args number
        {"rd",   shell::command{shell::command::actions::READ,    0}},  //todo: set required args number
        {"wr",   shell::command{shell::command::actions::WRITE,   0}},  //todo: set required args number
        {"sk",   shell::command{shell::command::actions::SEEK,    0}},  //todo: set required args number
        {"dr",   shell::command{shell::command::actions::DIR,     0}},  //todo: set required args number
        {"in",   shell::command{shell::command::actions::INIT,    5}},
        {"sv",   shell::command{shell::command::actions::SAVE,    0, 1}},
        {"help", shell::command{shell::command::actions::HELP,    0}},
        {"exit", shell::command{shell::command::actions::EXIT,    0}},
};

#ifdef FS_SHELL_MAIN
int main() {
    shell::run();
    return 0;
}
#endif
