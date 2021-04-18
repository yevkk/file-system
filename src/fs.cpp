#include "fs.hpp"

#include <fstream>
#include <cassert>
#include <utility>

namespace lab_fs {

    io::io(std::size_t blocks_no, std::size_t block_size, std::vector<std::vector<std::byte>> &&disk) :
            blocks_no{blocks_no},
            block_size{block_size},
            ldisk{disk} {}

    void io::read_block(std::size_t i, std::vector<std::byte>::iterator dest) {
        assert(i < blocks_no);
        std::copy(ldisk[i].cbegin(), ldisk[i].cend(), dest);
    }

    void io::write_block(std::size_t i, std::vector<std::byte>::iterator src) {
        assert(i < blocks_no);
        std::copy(src, src + (int) block_size, ldisk[i].begin());
    }

    std::size_t io::get_blocks_no() const {
        return blocks_no;
    }

    std::size_t io::get_block_size() const {
        return block_size;
    }

    file_system::file_descriptor::file_descriptor(std::size_t length,
                                                  const std::array<std::size_t, constraints::max_blocks_per_file> &occupied_blocks) :
            length{length},
            occupied_blocks{occupied_blocks} {}

    file_system::oft_entry::oft_entry(std::string filename, std::size_t descriptor_index) :
            filename{std::move(filename)},
            descriptor_index{descriptor_index},
            current_pos{0},
            modified{false} {}

    std::size_t file_system::oft_entry::get_descriptor_index() const {
        return descriptor_index;
    }

    std::string file_system::oft_entry::get_filename() const {
        return filename;
    }

    file_system::file_system(std::string filename, io &&disk_io) :
            filename{std::move(filename)},
            disk_io{disk_io},
            available_blocks(disk_io.get_blocks_no()) {
        std::vector<std::byte> buffer(disk_io.get_block_size());

        disk_io.read_block(0, buffer.begin());
        for (std::size_t i = 0; i < available_blocks.size(); i++) {
            available_blocks[i] = (bool) ((buffer[i / 8] >> (7 - (i % 8))) & std::byte{1});
        }

        oft.push_back(new oft_entry{"", 0});
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

        descriptors_map[0] = new file_descriptor(length, occupied_blocks);
    }

    fs_result file_system::create(const std::string& filename){
        if(get_dir_entry(filename) == -1)
            return EXISTS;
        for(std::size_t i = 3; i< available_blocks.size(); ++i) // TODO : i=k, currently k is hardcoded
            // available_blocks = bitmap, 0 is for free block?
            if(!available_blocks[i]){
                available_blocks[i] = true;
                
                for(std::size_t j = 0; j < 10000000; ++j) { // TODO: check all descriptors
                    if (descriptors_map.find(j) != descriptors_map.end()) // check in cashe
                        continue;
                    auto descriptor = get_descriptor(j);
                    if(true) // TODO: somehow check if descriptor is empty
                        if(write_dir_entry(filename,j))
                            return SUCCESS;
                        else 
                            // no free directory entries
                            return NOSPACE;
                }
                
                // no free descriptors
                return NOSPACE;
            }
        // no free blocks 
        return NOSPACE;
    }

    std::pair<std::size_t, fs_result> file_system::open(const std::string& filename){
        auto i = get_dir_entry(filename);
        if (i == -1)
            return {-1, NOTFOUND};
        
        auto descriptor = get_descriptor(i);
        if (descriptor == nullptr)
            return {-1, NOTFOUND};

        oft.emplace_back(filename,i);
        descriptors_map.insert({i,descriptor});
        descriptor_indexes_map.insert({filename,i});
        return {oft.size()-1,SUCCESS};
    }

    fs_result file_system::write(std::size_t i, const std::vector<std::byte>& src){

    }

    void file_system::save() {
        std::ofstream file{filename, std::ios::out | std::ios::binary};

        std::vector<std::byte> bitmap_block;
        std::uint8_t x = 0;
        for (std::size_t i = 0, j = 7; i < available_blocks.size(); i++, j--) {
            x |= available_blocks[i];
            if (j != 0) {
                x <<= 1;
            } else {
                bitmap_block.push_back(std::byte{x});
                x = 0;
                j = 7;
            }
        }
        bitmap_block.resize(disk_io.get_block_size(), std::byte{0});

        //todo: for every opened and modified file call save?

        file.write(reinterpret_cast<char *>(bitmap_block.data()), disk_io.get_block_size());
        std::vector<std::byte> block(disk_io.get_block_size());
        for (std::size_t i = 1; i < disk_io.get_blocks_no(); i++) {
            disk_io.read_block(i, block.begin());
            file.write(reinterpret_cast<char *>(bitmap_block.data()), disk_io.get_block_size());
        }
    }

    std::pair<file_system *, init_result> init(std::size_t cylinders_no,
                                               std::size_t surfaces_no,
                                               std::size_t sections_no,
                                               std::size_t section_length,
                                               const std::string &filename) {
        assert(cylinders_no > 0 && "number of cylinders should be positive integer");
        assert(surfaces_no > 0 && "number of surfaces should be positive integer");
        assert(sections_no > 0 && "number of sections should be positive integer");
        assert((section_length & 1) != 1 && "section (block) length should be power of 2");

        std::uint8_t blocks_no = cylinders_no * surfaces_no * sections_no;
        assert(blocks_no > 2 && "blocks number should be greater than 2");

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
            for (auto i = constrs::bytes_for_file_length; i < constrs::bytes_for_file_length + constrs::max_blocks_per_file; i++) {
                disk[1][i] = std::byte{255};
            }
        }

        return {new file_system{filename, io{blocks_no, section_length, std::move(disk)}}, result};
    }
} //namespace lab_fs
