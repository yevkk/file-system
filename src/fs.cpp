#include "fs.hpp"

#include <cassert>

namespace lab_fs {
    ldisk::ldisk(std::size_t blocks_no, std::size_t block_size, std::vector<std::vector<std::byte>> &&data) :
            blocks_no{blocks_no},
            block_size{block_size},
            data{data} {}

    void ldisk::read_block(std::size_t i, std::vector<std::byte>::iterator dest) {
        assert(i < blocks_no);
        std::copy(data[i].cbegin(), data[i].cend(), dest);
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

    oft_entry::oft_entry(std::size_t descriptor_index) :
        descriptor_index{descriptor_index},
        current_pos{0},
        modified{false} {}

    std::size_t oft_entry::get_descriptor_index() {
        return descriptor_index;
    }

} //namespace lab_fs
