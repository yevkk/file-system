#include "fs.hpp"

namespace lab_fs {
    file_system::file_descriptor *file_system::get_descriptor(std::size_t index, bool disable_caching) {
        if (_descriptors_cache.contains(index)) {
            return _descriptors_cache[index];
        }

        std::size_t offset = index * constraints::bytes_for_descriptor;
        std::uint8_t block_i = 1 + offset / _io.get_block_size();
        if (block_i >= constraints::descriptive_blocks_no) {
            return nullptr;
        }

        utils::disk_view dv{_io, block_i, false};
        if (dv.block_i() == constraints::descriptive_blocks_no - 1 && offset % _io.get_block_size() > _io.get_block_size() - constraints::bytes_for_descriptor) {
            return nullptr;
        }

        auto length = std::to_integer<std::size_t>(dv[offset++]);
        std::array<std::size_t, constraints::max_blocks_per_file> occupied_blocks{};
        for (unsigned i = 1; i < constraints::bytes_for_file_length; i++, offset++) {
            length += (std::to_integer<std::size_t>(dv[offset]) << 8);
        }
        for (unsigned i = 0; i < constraints::max_blocks_per_file; i++, offset++) {
            occupied_blocks[i] = std::to_integer<std::size_t>(dv[offset]);
        }

        if (!(length == 0 && std::all_of(occupied_blocks.begin(), occupied_blocks.end(), [](const auto &value) { return value == 0; }))) {
            auto fd = new file_descriptor(length, occupied_blocks);
            if (!disable_caching) {
                _descriptors_cache[index] = fd;
            }
            return fd;
        } else {
            return nullptr;
        }
    }

    bool file_system::save_descriptor(std::size_t index, file_descriptor *descriptor) {
        std::size_t offset = index * constraints::bytes_for_descriptor;
        std::uint8_t block_i = 1 + offset / _io.get_block_size();
        if (block_i >= constraints::descriptive_blocks_no) {
            return false;
        }

        utils::disk_view dv{_io, block_i, true};
        if (dv.block_i() == constraints::descriptive_blocks_no - 1 && offset % _io.get_block_size() > _io.get_block_size() - constraints::bytes_for_descriptor) {
            return false;
        }

        std::size_t length = descriptor->length;
        for (unsigned i = 0; i < constraints::bytes_for_file_length; i++, offset++) {
            dv[offset] = std::byte{(std::uint8_t) (length % 256)};
            length >>= 8;
        }
        for (unsigned i = 0; i < constraints::max_blocks_per_file; i++, offset++) {
            dv[offset] = std::byte{(std::uint8_t) descriptor->occupied_blocks[i]};
        }
        dv.push_buffer();

        return true;
    }

    int file_system::take_descriptor() {
        int index = 0;
        std::size_t offset = 0;
        utils::disk_view dv{_io, 1, false};

        while (true) {
            if (dv.block_i() == constraints::descriptive_blocks_no - 1 && offset % _io.get_block_size() > _io.get_block_size() - constraints::bytes_for_descriptor) {
                return -1;
            }

            bool found = true;
            for (unsigned i = 0; i < constraints::bytes_for_descriptor; i++) {
                if (dv[offset + i] != std::byte{0}) {
                    found = false;
                    break;
                }
            }

            if (found) {
                dv.enable_write();
                for (unsigned i = constraints::bytes_for_file_length; i < constraints::bytes_for_descriptor; i++) {
                    dv[offset + i] = std::byte{255};
                }
                dv.push_buffer();
                return index;
            } else {
                index++;
                offset += constraints::bytes_for_descriptor;
            }
        }
    }

    namespace utils {
        class dir_entry {
        public:
            static constexpr std::size_t dir_entry_size = file_system::constraints::max_filename_length + 1;

        public:
            dir_entry(const dir_entry &d) = default;

            dir_entry(std::string filename, std::byte descriptor_index) :
                    filename{std::move(filename)},
                    descriptor_index{descriptor_index} {}

            explicit dir_entry(std::vector<std::byte> &container) {
                filename = "";
                for (int i = 0; i < file_system::constraints::max_filename_length; i++) {
                    if (container[i] == std::byte{0}) {
                        break;
                    }
                    filename.push_back(char(container[i]));
                }
                descriptor_index = container[dir_entry_size - 1];
            }

            bool is_empty() const {
                return filename.empty() && (descriptor_index == std::byte{0});
            }

            std::vector<std::byte> convert() {
                std::vector<std::byte> container(dir_entry_size);
                for (unsigned i = 0; i < filename.size(); i++) {
                    container[i] = std::byte{(std::uint8_t) filename[i]};
                }
                for (unsigned i = filename.size(); i < file_system::constraints::max_filename_length; i++) {
                    container[i] = std::byte{0};
                }
                container[dir_entry_size - 1] = descriptor_index;
                return container;
            }

            static std::optional<dir_entry> read_dir_entry(file_system *fs, std::size_t i) {
                std::size_t pos = i * (dir_entry_size);
                if (fs->lseek(0, pos) == SUCCESS) {
                    std::vector<std::byte> container(dir_entry_size);

                    if (fs->read(0, container.begin(), utils::dir_entry::dir_entry_size).first == utils::dir_entry::dir_entry_size) {
                        return std::optional<dir_entry>{dir_entry(container)};
                    } else {
                        return std::nullopt;
                    }
                } else {
                    return std::nullopt;
                }
            }

        public:
            std::string filename;
            std::byte descriptor_index;
        };

    }  // namespace utils

    int file_system::get_descriptor_index_from_dir_entry(const std::string &filename) {
        std::size_t i = 0;
        while (true) {
            auto dire_opt = utils::dir_entry::read_dir_entry(this, i);
            if (!dire_opt.has_value()) {
                return -1;
            }
            auto dire = dire_opt.value();
            if (dire.filename == filename) {
                return int(dire.descriptor_index);
            }
            ++i;
        }
    }

    // picks last free space and reads through all to verify there is no same file
    std::pair<std::size_t, fs_result> file_system::take_dir_entry(const std::string &filename) {
        if (!_oft[0]->initialized) {
            if(auto res = initialize_oft_entry(_oft[0], 0); res != SUCCESS) {
                return {0,res};
            }
        }

        std::size_t i = 0;
        int free = -1;
        while (true) {
            auto dire_opt = utils::dir_entry::read_dir_entry(this, i);

            // looked through all dir entries and none of them is free
            if (!dire_opt.has_value()) {
                if (free == -1) {
                    // the file is just too big
                    if( _descriptors_cache[_oft[0]->get_descriptor_index()]->length == _io.get_block_size()* constraints::max_blocks_per_file) {
                        return {0, NO_SPACE};                       
                    // all entries were present
                    } else {
                        return {i, SUCCESS};
                    }
                }
                return {free, SUCCESS};
            }

            auto dire = dire_opt.value();

            // check if file has same name
            if (dire.filename == filename) {
                return {0, EXISTS};
            }

            // remember empty slot
            if (dire.is_empty()) {
                free = i;
            }
            i++;
        }
    }

    bool file_system::save_dir_entry(std::size_t i, std::string filename, std::size_t descriptor_index) {
        std::size_t pos = i * (utils::dir_entry::dir_entry_size);
        if (lseek(0, pos) == SUCCESS) {
            auto data = utils::dir_entry{std::move(filename), std::byte{(std::uint8_t) descriptor_index}}.convert();
            if (write(0, data.begin(), data.size()).second == SUCCESS) {
                return true;
            } else {
                return false;
            }
        } else {
            return false;
        }
    }

    bool file_system::allocate_block(file_descriptor *descriptor, std::size_t block_index) {
        for (int i = constraints::descriptive_blocks_no; i < _io.get_blocks_no(); i++) {
            if (!_bitmap[i]) {
                _bitmap[i] = true;
                descriptor->occupied_blocks[block_index] = i;
                return true;
            }
        }
        return false;
    }

    // the only function that explicitly changes current block 
    auto file_system::initialize_oft_entry(oft_entry* oft, std::size_t block) -> fs_result {
        auto descriptor = _descriptors_cache[oft->get_descriptor_index()];

        if (!oft->initialized || oft->current_block != block) {
            if (descriptor->is_initialized()) {
                if (descriptor->occupied_blocks[block] != 0) {
                    if (oft->modified) {
                        save_block(oft, oft->current_block);
                    }
                    _io.read_block(descriptor->occupied_blocks[block], oft->buffer.begin());
                    oft->modified = false;                   
                } else {
                    if(!allocate_block(descriptor, block)) {
                        return NO_BLOCK;
                    }

                    // doesn't save if there was an error
                    if (oft->modified) {
                        save_block(oft, oft->current_block);
                    }
                    save_descriptor(oft->get_descriptor_index(), descriptor);
                    oft->buffer = std::vector<std::byte>(_io.get_block_size(), std::byte{0});
                }
            } else {
                if (auto res = initialize_file_descriptor(descriptor, block); res != SUCCESS) {
                    save_descriptor(oft->get_descriptor_index(), descriptor);
                    return res;
                }
                oft->buffer = std::vector<std::byte>(_io.get_block_size(), std::byte{0});
            }
            oft->initialized = true;
            oft->current_block = block;
        }

        return SUCCESS;
    }

    fs_result file_system::initialize_file_descriptor(file_descriptor* descriptor, std::size_t block) {
        if (allocate_block(descriptor, 0)) {
            for (int i = 1; i < constraints::max_blocks_per_file; i++) {
                descriptor->occupied_blocks[i] = 0;
            }
        } else {
            return NO_BLOCK;
        }
        return SUCCESS;
    }

    void file_system::save_block(oft_entry *entry, std::size_t block) {
        auto descriptor = _descriptors_cache[entry->get_descriptor_index()];
        _io.write_block(descriptor->occupied_blocks[block], entry->buffer.begin());
        entry->modified = false;
        entry->initialized = false;
    }

    fs_result file_system::overwrite_dir_entry(const std::string &filename) {
        int dir_entry_index = -1;
        std::size_t last_index = 0;
        for (; ; last_index++) {
            auto entry = utils::dir_entry::read_dir_entry(this, last_index);
            if (!entry.has_value()) {
                break;
            } else if (entry.value().filename == filename) {
                dir_entry_index = last_index;
            }
        }

        if (last_index == 0 || dir_entry_index == -1)
            return NOT_FOUND;
        --last_index;

        auto last_entry = utils::dir_entry::read_dir_entry(this, last_index);

        if (!save_dir_entry(dir_entry_index, last_entry->filename, static_cast<size_t>(last_entry->descriptor_index))) {
            return FAIL;
        }

        if (!save_dir_entry(last_index, "", 0)) {
            return FAIL;
        }

        return SUCCESS;
    }

} //namespace lab_fs
