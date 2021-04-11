#include "fs.hpp"

#include <fstream>
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

    file_system::oft_entry::oft_entry(std::size_t descriptor_index) :
            descriptor_index{descriptor_index},
            current_pos{0},
            modified{false} {}

    std::size_t  file_system::oft_entry::get_descriptor_index() const {
        return descriptor_index;
    }

    void file_system::save() {
        std::ofstream file(filename, std::ios::out | std::ios::binary);

        std::vector<std::byte> bitmap_block;
        std::uint8_t x = 0;
        for (std::size_t i = 0, j = 8; i < available_blocks.size(); i++, j--) {
            x |= available_blocks[i];
            if (j != 0) {
                x <<= 1;
            } else {
                bitmap_block.push_back(std::byte{x});
                x = 0;
                j = 8;
            }
        }
        bitmap_block.resize(disk.get_block_size(), std::byte{0});

        //todo: for every opened and modified file call save???

        file.write(reinterpret_cast<char *>(bitmap_block.data()), disk.get_block_size());
        std::vector<std::byte> block(disk.get_block_size());
        for (std::size_t i = 1; i < disk.get_blocks_no(); i++) {
            disk.read_block(i, block.begin());
            file.write(reinterpret_cast<char *>(bitmap_block.data()), disk.get_block_size());
        }
    }
} //namespace lab_fs
