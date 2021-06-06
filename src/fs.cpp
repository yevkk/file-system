#include "fs.hpp"
#include "fs_utils.cpp"

#include <cassert>
#include <fstream>
#include <optional>

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
            current_block{0},
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
                char *char_data = new char[section_length];
                file.read(char_data, section_length);
                for (std::size_t j = 0; j < section_length; j++) {
                    disk[i][j] = std::byte{(std::uint8_t) char_data[j]};
                }
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

    void file_system::save(const std::string &filename) {
        std::ofstream file{filename, std::ios::out | std::ios::binary};

        std::vector<std::byte> bitmap_block;
        std::uint8_t x = 0;
        for (std::size_t i = 0, j = 7; i < _bitmap.size(); i++, j--) {
            x |= (bool) _bitmap[i];
            if (j != 0) {
                x <<= 1;
            } else {
                bitmap_block.push_back(std::byte{x});
                x = 0;
                j = 7;
            }
        }
        bitmap_block.resize(_io.get_block_size(), std::byte{0});

        for (std::uint8_t i = 0; i < _oft.size(); i++) {
            close(i);
        }

        file.write(reinterpret_cast<char *>(bitmap_block.data()), _io.get_block_size());
        std::vector<std::byte> block(_io.get_block_size());
        for (std::size_t i = 1; i < _io.get_blocks_no(); i++) {
            _io.read_block(i, block.begin());
            file.write(reinterpret_cast<char *>(block.data()), _io.get_block_size());
        }
    }

    void file_system::save() {
        save(_filename);
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

        if(save_dir_entry(index, filename, descriptor_index)) {
            return SUCCESS;
        } else {
            return FAIL;
        }
    }

    std::pair<std::size_t, fs_result> file_system::open(const std::string &filename) {
        if (filename.size() > constraints::max_filename_length) {
            return {0, INVALID_NAME};
        }

        std::size_t free_entry = 0;
        for (unsigned i = 0; i < _oft.size(); i++) {
            if (_oft[i] != nullptr) {
                if (_oft[i]->get_filename() == filename) {
                    return {0, ALREADY_OPENED};
                }
            } else {
                if (free_entry == 0) {
                    free_entry = i;
                }
            }
        }

        if (free_entry == 0 && _oft.size() == constraints::oft_max_size) {
            return {0, OFT_FULL};
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

        if (free_entry == 0) {
            _oft.emplace_back(new oft_entry{filename, (std::size_t) index});
            free_entry = _oft.size() - 1;
        } else {
            _oft[free_entry] = new oft_entry{filename, (std::size_t)index};
        }
        return {free_entry, SUCCESS};
    }

    fs_result file_system::destroy(const std::string& filename) {
        int descriptor_index = -1;

        // remove oft entry
        for (int i = 0; i < _oft.size(); ++i) {
            if (_oft[i] && _oft[i]->get_filename() == filename) {
                descriptor_index = (int) _oft[i]->get_descriptor_index();
                delete _oft[i];
                _oft[i] = nullptr;
            }
        }

        // file wasn't opened
        if (descriptor_index == -1) {
            // check if file info is already cached
            if (auto it = _descriptor_indexes_cache.find(filename); it != _descriptor_indexes_cache.end()) {
                descriptor_index = (int) it->second;
            } else {
                descriptor_index = get_descriptor_index_from_dir_entry(filename);
                if (descriptor_index == -1)
                    return NOT_FOUND;
            }
        }

        if (file_descriptor* descriptor = get_descriptor(descriptor_index)) {

            // clear caches
            _descriptors_cache.erase(descriptor_index);
            _descriptor_indexes_cache.erase(filename);


            // update available blocks in bitmap
            for (std::size_t i = 0; i < descriptor->length / _io.get_block_size(); ++i) {
                _bitmap[descriptor->occupied_blocks[i]] = false;
            }

            // clear descriptor in io
            file_descriptor empty_descriptor{0, {0, 0, 0}};
            save_descriptor(descriptor_index, &empty_descriptor);

            if (auto code = overwrite_dir_entry(filename); code != SUCCESS) {
                return code;
            }

            return SUCCESS;
        }

        return NOT_FOUND;
    }

    std::pair<size_t, fs_result> file_system::write(std::size_t i, std::vector<std::byte>::iterator mem_area, std::size_t count) {
        if (i >= _oft.size()) {
            return {0, NOT_FOUND};
        }
        if (count == 0) {
            return {0, SUCCESS};
        }
        auto ofte = _oft[i];
        if (!ofte)
            return {0, NOT_FOUND};
        auto descriptor = _descriptors_cache[ofte->get_descriptor_index()];
        std::size_t pos = ofte->current_pos % _io.get_block_size();
        std::size_t new_pos = pos;
        std::size_t offset = 0;
        bool changed = false;
        std::size_t current_block = ofte->current_pos / _io.get_block_size();

        if (ofte->current_pos == _io.get_block_size() * constraints::max_blocks_per_file) {
            return {0, TOO_BIG};
        }

        if (auto init_oft_res = initialize_oft_entry(ofte, current_block); init_oft_res != SUCCESS) {
            return {0, init_oft_res};
        }

        while (true) {
            // fits within current block
            if (count - offset <= _io.get_block_size() - pos) {
                std::copy(mem_area + offset, mem_area + count, ofte->buffer.begin() + pos);
                ofte->modified = true;
                ofte->current_pos += count - offset;

                /* if (ofte->current_pos / _io.get_block_size() > current_block) {
                    save_block(ofte, current_block);
                } */

                if (descriptor->length < ofte->current_pos) {
                    descriptor->length = ofte->current_pos;
                    save_descriptor(ofte->get_descriptor_index(), descriptor);
                }

                return {count, SUCCESS};
            }
            // src would be split between couple blocks
            else {
                auto part = _io.get_block_size() - pos;
                std::copy(mem_area + offset, mem_area + offset + part, ofte->buffer.begin() + pos);
                offset += part;
                ofte->current_pos += part;

                /*save_block(ofte, current_block);*/

                // check if there is space to continue
                if (current_block < constraints::max_blocks_per_file - 1) {
                    current_block++;
                    auto res = initialize_oft_entry(ofte, current_block);
                    if (res != SUCCESS) {
                        if (descriptor->length < ofte->current_pos) {
                            descriptor->length = ofte->current_pos;
                            save_descriptor(ofte->get_descriptor_index(), descriptor);
                        }
                        return {offset, res};
                    }
                    pos = 0;
                }
                // file has reached the max size
                else {
                    if (descriptor->length < constraints::max_blocks_per_file * _io.get_block_size()) {
                        descriptor->length = constraints::max_blocks_per_file * _io.get_block_size();
                        save_descriptor(ofte->get_descriptor_index(), descriptor);
                    }
                    return {offset, TOO_BIG};
                }
            }
        }
    }

    fs_result file_system::lseek(std::size_t i, std::size_t pos) {
        if (i >= _oft.size()) {
            return NOT_FOUND;
        }

        auto ofte = _oft[i];
        if (!ofte)
            return NOT_FOUND;

        auto descriptor = get_descriptor(ofte->get_descriptor_index());
        if (pos > descriptor->length) {
            return INVALID_POS;
        }

        /* std::size_t current_block = ofte->current_pos / _io.get_block_size();
        std::size_t new_block = pos / _io.get_block_size();
        if (current_block != new_block && ofte->modified) {
            save_block(ofte, current_block);
        } */
        ofte->current_pos = pos;
        return SUCCESS;
    }

    std::pair<std::size_t, fs_result> file_system::read(std::size_t i, std::vector<std::byte>::iterator mem_area, std::size_t count) {
        if (i >= _oft.size()) {
            return {0, NOT_FOUND};
        }

        auto oft_entry = _oft[i];
        auto descriptor = get_descriptor(oft_entry->get_descriptor_index());

        if (!_oft[i] || !descriptor) {
            return {0, NOT_FOUND};
        }

        std::size_t  bytes_read = 0;
        count = std::min(descriptor->length - oft_entry->current_pos, count);
        while (count > 0) {
            // end of file
            if (oft_entry->current_pos == constraints::max_blocks_per_file * _io.get_block_size()) {
                break;
            }

            // init block in oft entry
            if (!oft_entry->initialized || oft_entry->current_block != oft_entry->current_pos / _io.get_block_size()) {
                const std::size_t block = oft_entry->current_pos / _io.get_block_size();
                const auto res = initialize_oft_entry(oft_entry, block);

                if (res != SUCCESS) {
                    return {0, res};
                }
            }

            const std::size_t position_in_block = oft_entry->current_pos % _io.get_block_size();
            const std::size_t n_bytes_to_copy = std::min(count, _io.get_block_size() - position_in_block);

            std::copy(oft_entry->buffer.begin() + position_in_block,
                      oft_entry->buffer.begin() + position_in_block + n_bytes_to_copy,
                      mem_area);

            oft_entry->current_pos += n_bytes_to_copy;

            /* if(oft_entry->current_pos % _io.get_block_size() == 0) {
                if (oft_entry->modified) {
                    save_block(oft_entry, oft_entry->current_pos / _io.get_block_size() - 1);
                } else {
                    oft_entry->initialized = false;
                }
            } */
            

            std::advance(mem_area, n_bytes_to_copy);
            count -= n_bytes_to_copy;
            bytes_read += n_bytes_to_copy;
        }

        return {bytes_read, SUCCESS};
    }

    fs_result file_system::close(std::size_t i) {
        if (i >= _oft.size() || !_oft[i]) {
            return NOT_FOUND;
        }
        auto oft_entry = _oft[i];
        const auto descriptor = get_descriptor(oft_entry->get_descriptor_index());

        if (oft_entry->modified) {
            // save
            const std::size_t current_block = oft_entry->current_pos / _io.get_block_size();
            _io.write_block(descriptor->occupied_blocks[current_block], oft_entry->buffer.begin());
        }

        delete _oft[i];
        _oft[i] = nullptr;

        return SUCCESS;
    }

    auto file_system::directory() -> std::vector<std::pair<std::string, std::size_t>> {
        std::vector<std::pair<std::string, std::size_t>> res;
        for (std::size_t i = 0; ; i++) {
            auto entry  = utils::dir_entry::read_dir_entry(this, i);
            if (!entry.has_value()) {
                break;
            } else if (!entry.value().filename.empty()) {
                res.emplace_back(entry.value().filename, get_descriptor(std::to_integer<std::size_t>(entry.value().descriptor_index), true)->length);
            }
        }
        return res;
    }

}  //namespace lab_fs
