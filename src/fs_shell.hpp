#pragma once

#include "fs.hpp"

#include <map>

class shell {
private:
    enum class commands {
        CREATE, DESTROY, OPEN, CLOSE, READ, WRITE, SEEK, DIR, INIT, SAVE, HELP, TERMINATE
    };

    static constexpr std::map<std::string, commands> commands_map = {
            {"cr", CREATE},
            {"de", DESTROY},
            {"op", OPEN},
            {"cl", CLOSE},
            {"rd", READ},
            {"wr", WRITE},
            {"sk", SEEK},
            {"dr", DIR},
            {"in", INIT},
            {"sv", SAVE},
            {"help", HELP},
            {"terminate", SAVE} //TODO: change to smth shortened e.g. "tm", "rt" or "stop" (?)
    };

public:
    cli() = delete;

    void run() {

    }
};