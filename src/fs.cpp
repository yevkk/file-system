#include "fs.hpp"

#include <cassert>
#include <fstream>

namespace lab_fs {

    file_system::file_descriptor::file_descriptor(std::size_t length, const std::array<std::size_t, constraints::max_blocks_per_file> &occupied_blocks) :
            length{length},
            occupied_blocks{occupied_blocks} {}

    bool file_system::file_descriptor::is_initialized() const {
        if (length > 0)
            return true;
        for (auto i : occupied_blocks) {
            if (i != 255) {
                return true;
            }
        }
        return false;
    }

    file_system::oft_entry::oft_entry(std::string filename, std::size_t descriptor_index) :
            _filename{std::move(filename)},
            _descriptor_index{descriptor_index},
            current_pos{0},
            modified{false},
            initialized{false} {}

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
            _bitmap[i] = (bool)((buffer[i / 8] >> (7 - (i % 8))) & std::byte{1});
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
            disk[0][0] = std::byte{192};  // 192 = 11000000

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

    fs_result file_system::create(const std::string &filename) {
        if (filename.size() > constraints::max_filename_length) {
            return INVALID_NAME;
        }

        auto result = take_dir_entry(filename);
        if (result.second != SUCCESS) {
            return result.second;
        }

        auto index = result.first;
        auto descriptor_index = take_descriptor();
        if (descriptor_index == -1)
            return NO_SPACE;

        save_dir_entry(index, filename, descriptor_index);
        return SUCCESS;
    }

    std::pair<std::size_t, fs_result> file_system::open(const std::string &filename) {
        if (filename.size() > constraints::max_filename_length) {
            return {0, INVALID_NAME};
        }

        if (_oft.size() == constraints::oft_max_size) {
            return {0, NO_SPACE};
        }

        int index;

        // check if file info is already cached
        if (_descriptor_indexes_cache.contains(filename)) {
            index = _descriptor_indexes_cache[filename];
        } else {
            index = get_descriptor_index_from_dir_entry(filename);
            if (index == -1)
                return {0, NOT_FOUND};
        }
        get_descriptor(index);
        _oft.emplace_back(new oft_entry{filename, (std::size_t) index});
        return {_oft.size() - 1, SUCCESS};
    }

    fs_result file_system::write(std::size_t i, const std::vector<std::byte> &src) {
        if (i > _oft.size()) {
            return NOT_FOUND;
        }
        if (src.empty()) {
            return SUCCESS;
        }
        auto ofte = _oft[i];
        auto buffer = ofte->buffer;
        auto descriptor = _descriptors_cache[ofte->get_descriptor_index()];
        std::size_t pos = ofte->current_pos % _io.get_block_size();
        std::size_t new_pos = pos;
        std::size_t offset = 0;
        bool changed = false;
        std::size_t current_block = ofte->current_pos / _io.get_block_size();

        if (ofte->current_pos == _io.get_block_size() * constraints::max_blocks_per_file) {
            return INVALID_POS;
        }
        if (!ofte->initialized) {
            buffer = std::vector<std::byte>(_io.get_block_size(), std::byte{0});
            if (descriptor->is_initialized()) {
                if (descriptor->occupied_blocks[current_block] != 0) {
                    _io.read_block(descriptor->occupied_blocks[current_block], buffer.begin());
                } else {
                    if(!allocate_block(descriptor,current_block)) {
                        return NO_SPACE;
                    }
                }
            } else {
                if (allocate_block(descriptor, 0)) {
                    for (int i = 1; i < constraints::max_blocks_per_file; i++) {
                        descriptor->occupied_blocks[i] = 0;
                    }
                    changed = true;
                } else {
                    return NO_SPACE;
                }
            }
            ofte->initialized = true;
        }

        while (true) {
            // fits within current block
            if (src.size() - offset <= _io.get_block_size() - pos) {
                std::copy(src.begin() + offset, src.end(), buffer.begin() + pos);
                ofte->modified = true;
                new_pos = pos + src.size() - offset;
                ofte->current_pos = current_block * _io.get_block_size() + new_pos;

                if (descriptor->length < current_block * _io.get_block_size() + new_pos) {
                    descriptor->length = current_block * _io.get_block_size() + new_pos;
                    changed = true;
                }

                if (changed) {
                    save_descriptor(ofte->get_descriptor_index(), descriptor);
                }
                return SUCCESS;
            }
            // src would be split between couple blocks
            else {
                auto part = _io.get_block_size() - pos;
                std::copy(src.begin() + offset, src.begin() + offset + part, buffer.begin() + pos);
                offset += part;
                new_pos = pos + part;

                //save changes to disk
                _io.write_block(descriptor->occupied_blocks[current_block], buffer.begin());
                ofte->modified = false;

                // check if there is space to continue
                if (current_block < constraints::max_blocks_per_file - 1) {
                    current_block++;
                    // try to read next allocated block
                    if (descriptor->occupied_blocks[current_block] != 0) {
                        _io.read_block(descriptor->occupied_blocks[current_block], buffer.begin());
                        pos = 0;
                    }
                    // try to allocate new block
                    else {
                        if (allocate_block(descriptor, current_block)) {
                            changed = true;
                            std::fill(buffer.begin(), buffer.end(), std::byte(0));
                        } else {
                            ofte->current_pos = current_block * _io.get_block_size();
                            if (descriptor->length < current_block * _io.get_block_size()) {
                                descriptor->length = current_block * _io.get_block_size();
                                changed = true;
                            }

                            if (changed) {
                                save_descriptor(ofte->get_descriptor_index(), descriptor);
                            }
                            return NO_SPACE;
                        }
                    }
                }
                // file has reached the max size
                else {
                    if (descriptor->length < constraints::max_blocks_per_file * _io.get_block_size()) {
                        descriptor->length = constraints::max_blocks_per_file * _io.get_block_size();
                        changed = true;
                    }
                    ofte->current_pos = descriptor->length;
                    if (changed) {
                        save_descriptor(ofte->get_descriptor_index(), descriptor);
                    }
                    return TOO_BIG;
                }
            }
        }
    }

    fs_result file_system::lseek(std::size_t i, std::size_t pos) {
        if (i > _oft.size()) {
            return NOT_FOUND;
        }

        auto ofte = _oft[i];
        auto descriptor = get_descriptor(ofte->get_descriptor_index());
        if (pos > descriptor->length) {
            return INVALID_POS;
        }

        std::size_t current_block = ofte->current_pos / _io.get_block_size();
        std::size_t new_block = pos / _io.get_block_size();
        if (current_block != new_block && ofte->modified) {
            _io.write_block(current_block, ofte->buffer.begin());
            ofte->initialized = false;
            ofte->modified = false;
        }
        ofte->current_pos = pos;
        return SUCCESS;
    }

    //todo: implement here destroy, close, read, directory...

}  //namespace lab_fs
