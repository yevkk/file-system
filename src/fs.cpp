#include "fs.hpp"

#include <cassert>

namespace lab_fs {
    ldisk::ldisk(std::size_t blocks_no, std::size_t block_size, std::vector<std::vector<std::byte>> &&data) :
            blocks_no{blocks_no},
            block_size{block_size},
            data{data} {}

    void ldisk::read_block(std::size_t i, std::vector<std::byte>::iterator dest) {
        assert(i < blocks_no);
        std::copy(data[i].begin(), data[i].end(), dest);
    }

    void ldisk::write_block(std::size_t i, std::vector<std::byte>::iterator src) {
        assert(i < blocks_no);
        std::copy(src, src + (int) block_size, data[i].begin());
    }

    std::size_t ldisk::get_blocks_no() const {
        return blocks_no;
    }

    std::size_t ldisk::get_block_size() const {
        return block_size;
    }
} //namespace lab_fs
