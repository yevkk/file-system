#pragma once

#include "fs.hpp"

#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <cstring>
#include <algorithm>

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

        command(actions action, unsigned args_min_no) : command(action, args_min_no, args_min_no) {};

        actions action;
        unsigned args_min_no;
        unsigned args_max_no;
    };

    static const std::map<std::string, const command> commands_map;
    static const std::map<lab_fs::fs_result, std::string> fs_results_map;

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

    static void run(std::istream &is = std::cin, bool repeat_commands = false) {
        lab_fs::file_system *fs = nullptr;
        while (true) {
            std::string line;
            std::getline(is, line);
            if (repeat_commands) {
                std::cout << line << "\n";
            }
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
                    if (args.size() != cmd.args_min_no + 1) {
                        std::cout << "error: wrong number of arguments\n";
                    }
                    auto res = fs->create(args[1]);
                    std::cout << fs_results_map.at(res) << std::endl;
                    break;
                }
                case command::actions::DESTROY: {
                    const std::string &filename = args[1];
                    auto code = fs->destroy(filename);
                    std::cout << fs_results_map.at(code) << ", destroy file " << filename << std::endl;

                    break;
                }
                case command::actions::OPEN: {
                    if (args.size() != cmd.args_min_no + 1) {
                        std::cout << "error: wrong number of arguments\n";
                    }
                    std::size_t index;
                    lab_fs::fs_result res;
                    std::tie(index, res) = fs->open(args[1]);
                    if (res != lab_fs::fs_result::SUCCESS) {
                        std::cout << fs_results_map.at(res) << std::endl;
                    } else {
                        std::cout << "file index = " << index << std::endl;
                    }
                    break;
                }
                case command::actions::CLOSE: {
                    std::size_t index;
                    try {
                        index = std::stoull(args[1]);
                    } catch (...) {
                        std::cout << "invalid argument for close command: " << args[1] << std::endl;
                        break;
                    }

                    auto code = fs->close(index);

                    std::cout << fs_results_map.at(code) << ", close file " << index << std::endl;

                    break;
                }
                case command::actions::READ: {
                    std::size_t index;
                    std::size_t count;
                    try {
                        index = std::stoull(args[1]);
                        count = std::stoull(args[2]);
                    } catch (...) {
                        std::cout << "invalid arguments for read command: " << args[1] << " " << args[2] << std::endl;
                        break;
                    }

                    std::vector<std::byte> content{count, std::byte{}};

                    auto[bytes_read, code] = fs->read(index, content.begin(), count);

                    std::cout << fs_results_map.at(code) << ", read " << count << " bytes" << std::endl;

                    break;
                }
                case command::actions::WRITE: {
                    if (args.size() != cmd.args_min_no + 1) {
                        std::cout << "error: wrong number of arguments\n";
                    }
                    std::size_t index = std::stoull(args[1]);
                    std::size_t length = std::stoull(args[2]);
                    std::vector<std::byte> src(length);
                    for (std::size_t i = 0; i < length; i++) {
                        src[i] = std::byte(i % 256);
                    }
                    std::size_t count;
                    lab_fs::fs_result res;
                    std::tie(count, res) = fs->write(index, src.begin(), src.size());
                    std::cout << fs_results_map.at(res) << ", written " << count << " bytes" << std::endl;
                    break;
                }
                case command::actions::SEEK: {
                    if (args.size() != cmd.args_min_no + 1) {
                        std::cout << "error: wrong number of arguments\n";
                    }
                    auto res = fs->lseek(std::stoull(args[1]), std::stoull(args[2]));
                    std::cout << fs_results_map.at(res) << std::endl;
                    break;
                }
                case command::actions::DIR: {
                    auto dir = fs->directory();
                    auto it = std::max_element(dir.begin(), dir.end(), [](auto a, auto b) {
                        return a.first.size() < b.first.size();
                    });
                    auto max_filename_length = (it == dir.end()) ? 0 : it->first.size();
                    for (auto &file : dir) {
                        file.first.resize(max_filename_length, ' ');
                        std::cout << file.first << " | " << file.second << "B\n";
                    }
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
                    std::cout << "cr <file_name> - create file\n";
                    std::cout << "de <file_name> - destroy file\n";
                    std::cout << "op <file_name> - open file\n";
                    std::cout << "cl <file_index> - close file\n";
                    std::cout << "rd <file_index> <number_of_bytes> - read from file\n";
                    std::cout << "wr <file_index> <number_of_bytes> - write to file (writes sequences 0,1,...,255,0,...)\n";
                    std::cout << "sk <file_index> <position> - seek to position in file\n";
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
        {"cr",   shell::command{shell::command::actions::CREATE,  1}},
        {"de",   shell::command{shell::command::actions::DESTROY, 1}},
        {"op",   shell::command{shell::command::actions::OPEN,    1}},
        {"cl",   shell::command{shell::command::actions::CLOSE,   1}},
        {"rd",   shell::command{shell::command::actions::READ,    2}},
        {"wr",   shell::command{shell::command::actions::WRITE,   2}},
        {"sk",   shell::command{shell::command::actions::SEEK,    2}},
        {"dr",   shell::command{shell::command::actions::DIR,     0}},
        {"in",   shell::command{shell::command::actions::INIT,    5}},
        {"sv",   shell::command{shell::command::actions::SAVE,    0, 1}},
        {"help", shell::command{shell::command::actions::HELP,    0}},
        {"exit", shell::command{shell::command::actions::EXIT,    0}},
};

const std::map<lab_fs::fs_result, std::string> shell::fs_results_map = {
    {lab_fs::fs_result::SUCCESS, "success"},
    {lab_fs::fs_result::EXISTS, "error: exists"},
    {lab_fs::fs_result::NO_SPACE, "error: no space"},
    {lab_fs::fs_result::NOT_FOUND, "error: not found"},
    {lab_fs::fs_result::TOO_BIG, "error: file is too big"},
    {lab_fs::fs_result::INVALID_NAME, "error: invalid name"},
    {lab_fs::fs_result::INVALID_POS, "error: invalid pos"},
    {lab_fs::fs_result::ALREADY_OPENED, "error: already opened"},
    {lab_fs::fs_result::FAIL, "error: something went wrong"},
};

#ifdef FS_SHELL_MAIN
int main() {
    shell::run();
    return 0;
}
#endif
