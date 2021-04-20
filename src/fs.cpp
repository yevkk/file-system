#include "fs.hpp"

#include <fstream>
#include <cassert>

namespace lab_fs {

    file_system::file_descriptor::file_descriptor(std::size_t length,
                                                  const std::array<std::size_t, constraints::max_blocks_per_file> &occupied_blocks)
            :
            length{length},
            occupied_blocks{occupied_blocks} {}

    file_system::oft_entry::oft_entry(std::string filename, std::size_t descriptor_index) :
            _filename{std::move(filename)},
            _descriptor_index{descriptor_index},
            current_pos{0},
            modified{false} {}

    std::size_t file_system::oft_entry::get_descriptor_index() const {
        return _descriptor_index;
    }

    std::string file_system::oft_entry::get_filename() const {
        return _filename;
    }

    file_system::file_system(std::string filename, io &&disk_io) :
            _filename{std::move(filename)},
            _io{disk_io},
            _bitmap(disk_io.get_blocks_no()) {
        std::vector<std::byte> buffer(disk_io.get_block_size());

        disk_io.read_block(0, buffer.begin());
        for (std::size_t i = 0; i < _bitmap.size(); i++) {
            _bitmap[i] = (bool) ((buffer[i / 8] >> (7 - (i % 8))) & std::byte{1});
        }

        _oft.push_back(new oft_entry{"", 0});
        disk_io.read_block(1, buffer.begin());
        std::uint8_t pos = 0;

        auto length = std::to_integer<std::size_t>(buffer[pos]);
        for (pos++; pos < file_system::constraints::bytes_for_file_length; pos++) {
            length <<= 8;
            length += std::to_integer<std::size_t>(buffer[pos]);
        }

        std::array<std::size_t, constraints::max_blocks_per_file> occupied_blocks{};
        for (unsigned i = 0; i < constraints::max_blocks_per_file; i++, pos++) {
            occupied_blocks[i] = std::to_integer<std::size_t>(buffer[pos]);
        }

        _descriptors_cache[0] = new file_descriptor(length, occupied_blocks);
    }

    std::pair<file_system *, init_result> file_system::init(std::size_t cylinders_no,
                                                            std::size_t surfaces_no,
                                                            std::size_t sections_no,
                                                            std::size_t section_length,
                                                            const std::string &filename) {
        assert(cylinders_no > 0 && "number of cylinders should be positive integer");
        assert(surfaces_no > 0 && "number of surfaces should be positive integer");
        assert(sections_no > 0 && "number of sections should be positive integer");
        assert((section_length & 1) != 1 && "section (block) length should be power of 2");

        std::uint8_t blocks_no = cylinders_no * surfaces_no * sections_no;
        assert(blocks_no > constraints::descriptive_blocks_no && "blocks number is too small");

        std::vector disk{blocks_no, std::vector{section_length, std::byte{0}}};
        init_result result;

        std::ifstream file(filename, std::ios::in | std::ios::binary);
        if (file.is_open()) {
            result = RESTORED;
            for (std::size_t i = 0; i < blocks_no; i++) {
                file.read(reinterpret_cast<char *>(disk[i].data()), section_length);
            }
            //todo: consider meeting end of file?
        } else {
            result = CREATED;
            disk[0][0] = std::byte{224}; // 224 = 1110000

            using constrs = file_system::constraints;
            for (auto i = constrs::bytes_for_file_length;
                 i < constrs::bytes_for_file_length + constrs::max_blocks_per_file; i++) {
                disk[1][i] = std::byte{255};
            }
        }

        return {new file_system{filename, io{blocks_no, section_length, std::move(disk)}}, result};
    }

    void file_system::save() {
        std::ofstream file{_filename, std::ios::out | std::ios::binary};

        std::vector<std::byte> bitmap_block;
        std::uint8_t x = 0;
        for (std::size_t i = 0, j = 7; i < _bitmap.size(); i++, j--) {
            x |= _bitmap[i];
            if (j != 0) {
                x <<= 1;
            } else {
                bitmap_block.push_back(std::byte{x});
                x = 0;
                j = 7;
            }
        }
        bitmap_block.resize(_io.get_block_size(), std::byte{0});

        //todo: for every opened and modified file call save?

        file.write(reinterpret_cast<char *>(bitmap_block.data()), _io.get_block_size());
        std::vector<std::byte> block(_io.get_block_size());
        for (std::size_t i = 1; i < _io.get_blocks_no(); i++) {
            _io.read_block(i, block.begin());
            file.write(reinterpret_cast<char *>(bitmap_block.data()), _io.get_block_size());
        }
    }

    file_system::file_descriptor *file_system::get_descriptor(std::size_t index) {
        if (_descriptors_cache.contains(index)) {
            return _descriptors_cache[index];
        }

        std::size_t offset = index * constraints::bytes_for_descriptor;
        std::uint8_t block_index = 1 + offset / _io.get_block_size();
        std::size_t pos = offset % _io.get_block_size();

        if (block_index >= constraints::descriptive_blocks_no || (block_index == constraints::descriptive_blocks_no - 1 && pos > _io.get_block_size() - constraints::bytes_for_descriptor)) {
            return nullptr;
        }

        std::size_t length = 0;
        std::array<std::size_t, constraints::max_blocks_per_file> occupied_blocks{};

        std::vector<std::byte> buffer{_io.get_block_size()};
        _io.read_block(block_index, buffer.begin());

        for (unsigned i = 0; i < constraints::bytes_for_file_length; i++, pos++) {
            if (pos == buffer.size()) {
                _io.read_block(block_index + 1, buffer.begin());
                pos = 0;
            }
            if (i != 0) {
                length <<= 8;
            }
            length += std::to_integer<std::size_t>(buffer[pos]);
        }

        for (unsigned i = 0; i < constraints::max_blocks_per_file; i++, pos++) {
            if (pos == buffer.size()) {
                _io.read_block(block_index + 1, buffer.begin());
                pos = 0;
            }
            occupied_blocks[i] = std::to_integer<std::size_t>(buffer[pos]);
        }

        if (!(length == 0 && std::all_of(occupied_blocks.begin(), occupied_blocks.end(),
                                         [](const auto &value) { return value == 0; }))) {
            auto fd = new file_descriptor(length, occupied_blocks);
            _descriptors_cache[index] = fd;
            return fd;
        } else {
            return nullptr;
        }
    }

    bool file_system::save_descriptor(std::size_t index, file_descriptor *descriptor) {
        std::size_t offset = index * constraints::bytes_for_descriptor;
        std::size_t pos = offset % _io.get_block_size();
        std::uint8_t block_index = 1 + offset / _io.get_block_size();

        if (block_index >= constraints::descriptive_blocks_no || (block_index == constraints::descriptive_blocks_no - 1 && pos > _io.get_block_size() - constraints::bytes_for_descriptor)) {
            return false;
        }

        std::vector<std::byte> buffer{_io.get_block_size()};
        _io.read_block(block_index, buffer.begin());

        std::size_t length = descriptor->length;
        for (unsigned i = 0; i < constraints::bytes_for_file_length; i++, pos++) {
            if (pos == buffer.size()) {
                _io.write_block(block_index, buffer.begin());
                block_index++;
                pos = 0;
                _io.read_block(block_index, buffer.begin());
            }
            buffer[pos] = std::byte{(std::uint8_t) (length % 256)};
            length >>= 8;
        }

        for (unsigned i = 0; i < constraints::max_blocks_per_file; i++, pos++) {
            if (pos == buffer.size()) {
                _io.write_block(block_index, buffer.begin());
                block_index++;
                pos = 0;
                _io.read_block(block_index, buffer.begin());
            }
            buffer[pos] = std::byte{(std::uint8_t) descriptor->occupied_blocks[i]};
        }

        _io.write_block(block_index, buffer.begin());

        return true;
    }

    /**
     * @warning todo: working with several blocks for descriptors is not implemented yet
     */
    int file_system::take_descriptor() {
        std::vector<std::byte> buffer{_io.get_block_size()};
        std::size_t pos = 0;
        int index = 0;
        std::uint8_t block_index = 1;

        _io.read_block(block_index, buffer.begin());
        while (true) {
            if (block_index == constraints::descriptive_blocks_no - 1 && pos > _io.get_block_size() - constraints::bytes_for_descriptor) {
                return -1;
            }

            bool ready = true;
            for (unsigned i = 0; i < constraints::bytes_for_descriptor; i++) {
                if (buffer[pos + i] != std::byte{0}) {
                    ready = false;
                    break;
                }
            }

            if (ready) {
                for (unsigned i = constraints::bytes_for_file_length; i < constraints::bytes_for_descriptor; i++) {
                    buffer[pos + i] = std::byte{255};
                }
                _io.write_block(block_index, buffer.begin());
                return index;
            } else {
                index++;
                pos += constraints::bytes_for_descriptor;
            }
        }

    }

} //namespace lab_fs
