#pragma once

#include <vector>
#include <cstddef>

namespace lab_fs {

class ldisk {
public:
    ldisk(std::size_t blocks_no, std::size_t block_size, std::vector<std::vector<std::byte>>&& data);

    void read_block(std::size_t i, std::vector<std::byte>::iterator dest);
    void write_block(std::size_t i, std::vector<std::byte>::iterator src);

    [[nodiscard]] std::size_t get_blocks_no() const;
    [[nodiscard]] std::size_t get_block_size() const;

private:
    std::size_t blocks_no;
    std::size_t block_size;

    std::vector<std::vector<std::byte>> data;
};

class oft_entry {
public:
    oft_entry(std::size_t descriptor_index);

    [[nodiscard]] std::size_t get_descriptor_index();

    std::vector<std::byte> buffer; //TODO: think on public access to buffer and manually setting modified
    std::size_t current_pos;
    bool modified;
private:
    std::size_t descriptor_index;
};

} //namespace lab_fs