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

    namespace utils {
        struct dir_entry {
            char filename_[file_system::constraints::max_filename_length];
            std::byte descriptor_index_; 

            dir_entry(const std::string& filename, std::byte descriptor_index) :
                    descriptor_index_{descriptor_index}  
            {
                std::copy(filename.begin(),filename.begin() + file_system::constraints::max_filename_length, filename);
            }

            std::string get_filename(){
                return std::string(filename_);
            }

            bool is_empty() const {
                for(int i = 0; i < file_system::constraints::max_filename_length; ++i){
                    if(filename_[i] != 0)
                        return false;
                }
                return true;
            }
        };

        std::optional<dir_entry> read_dir_entry(file_system* fs ,std::size_t i) {
            if(i >= fs->max_files_quantity - 1){
                return std::nullopt;
            }
            
            auto container = std::vector<std::byte>(sizeof(dir_entry));
            
            std::size_t pos = i * (sizeof(dir_entry));
            if(fs->lseek(0, pos) == SUCCESS) {
                // TODO: replace with (fs->read(0, container) == SUCCESS) or something like that
                if(true){
                    std::byte* data = &container[0];
                    auto dire = reinterpret_cast<dir_entry*>(data);
                    return std::optional<dir_entry>{*dire};
                } else {
                    return std::nullopt;;
                }
            } else {
                return std::nullopt;;
            }
        }
    }

    int file_system::get_descriptor_index_from_dir_entry(const std::string& filename){
        std::size_t i = 0;
        while (true){
            auto dire_opt = utils::read_dir_entry(this,i);
            if (!dire_opt.has_value()){
                return -1;
            }
            auto dire = dire_opt.value();
            if (dire.get_filename() == filename){
                return int(dire.descriptor_index_);
            }
            ++i;
        }
    }

    // picks last free space and reads through all to verify there is no same file
    int file_system::take_dir_entry(const std::string& filename) {
        std::size_t i = 0;
        int free = -1;
        while (true){
            auto dire_opt = utils::read_dir_entry(this,i);
            
            // looked through all dir entries and none of them is free
            if (!dire_opt.has_value() && free == -1){
                return -2;
            }
            
            auto dire = dire_opt.value();
            
            // check if file has same name
            if(dire.get_filename() == filename){
                return -1;
            }

            // remember empty slot
            if (dire.is_empty()){
                free = i;
            }
            ++i;
        }
        return free;
    }  

    bool file_system::save_dir_entry(std::size_t i, std::string filename, std::size_t descriptor_index){
        if(i >= max_files_quantity - 1){
            return false;
        }
        auto dire = utils::dir_entry{filename,std::byte{descriptor_index}};
        auto aux = reinterpret_cast<std::byte*>(&dire);
        auto data = std::vector<std::byte>();
        data.insert(data.end(), aux, aux + sizeof(utils::dir_entry));

        std::size_t pos = i * (sizeof(utils::dir_entry));
        if(lseek(0, pos) == SUCCESS) {
            if(write(0, data) == SUCCESS){
                return true;
            } else {
                return false;
            }
        } else {
            return false;
        }
    }

    fs_result file_system::create(const std::string& filename){
        if(filename.size() > constraints::max_filename_length){
            return INVALIDNAME;
        }

        auto index = take_dir_entry(filename);
        if(index == -1) {
            return EXISTS;
        }
        if(index == -2) {
            return NOSPACE;
        }
        
        auto descriptor_index = take_descriptor();
        if(descriptor_index == -1)
            return NOSPACE;

        save_dir_entry(index,filename,descriptor_index);
        return SUCCESS;
    }

    std::pair<std::size_t, fs_result> file_system::open(const std::string& filename){       
        if(filename.size() > constraints::max_filename_length){
            return {0,INVALIDNAME};
        }
        
        if (_oft.size() == constraints::oft_max_size){
            return {0,NOSPACE};
        }
        
        int index;
        
        // check if file info is already cached
        if(_descriptor_indexes_cache.contains(filename)){
            index = _descriptor_indexes_cache[filename];
        } else {
            index = get_descriptor_index_from_dir_entry(filename);
            if(index == -1)
                return {0,NOTFOUND};          
        }
        get_descriptor(index);      
        _oft.emplace_back(filename,index);    
        return {_oft.size()-1,SUCCESS};
    }

    fs_result file_system::write(std::size_t i, const std::vector<std::byte>& src){
        if (src.size() == 0) {
            return SUCCESS;
        }        
        auto ofte = _oft[i];
        auto buffer = ofte->buffer;
        auto descriptor=  _descriptors_cache[ofte->get_descriptor_index()];
        std::size_t buff_pos = ofte->current_pos % _io.get_block_size();
        std::size_t current_block = ofte->current_pos / _io.get_block_size();
        std::size_t offset = 0;
        std::size_t new_pos = buff_pos;
        bool changed = false;

        if (!ofte->initialized){
            _io.read_block(descriptor->occupied_blocks[current_block],buffer.begin());
        }

        while(true){
            // fits within current block
            if (src.size() - offset < _io.get_block_size() - buff_pos) {
                std::copy(src.begin()+offset, src.end(), buffer.begin()+buff_pos);
                new_pos = buff_pos + src.size() - offset;
                ofte->current_pos = current_block * _io.get_block_size() + new_pos;

                if (descriptor->length < current_block * _io.get_block_size() + new_pos){
                    descriptor->length =  current_block * _io.get_block_size() + new_pos;
                    changed = true;
                }

                if (changed){
                    save_descriptor(ofte->get_descriptor_index(),descriptor);
                }
                return SUCCESS;
            }
            // src would be split between couple blocks 
            else {
                auto part = _io.get_block_size() - buff_pos;
                std::copy(src.begin()+offset, src.begin()+offset+part, buffer.begin()+buff_pos);
                offset += part;
                new_pos = buff_pos + part;

                //save changes to disk
                _io.write_block(descriptor->occupied_blocks[current_block],buffer.begin());
                
                // check if there is space to continue
                if(current_block < constraints::max_blocks_per_file - 1){
                    ++current_block;
                    // read next allocated block
                    if (descriptor->occupied_blocks[current_block] != 0){
                        _io.read_block(descriptor->occupied_blocks[current_block],buffer.begin());
                        buff_pos = 0;
                    }
                    // allocate new block
                    else {
                        for(int i = constraints::descriptive_blocks_no; i < _io.get_blocks_no(); ++i)
                            if (!_bitmap[i]){
                                _bitmap[i] = true;
                                descriptor->occupied_blocks[current_block] = i;
                                changed = true;
                                
                                // new fresh buffer!
                                std::fill(buffer.begin(),buffer.end(),std::byte(0));
                                break;
                            }
                        
                        // if new current block is still zero that means that all blocks are occupied!
                        if(descriptor->occupied_blocks[current_block] == 0){
                            ofte->current_pos = current_block * _io.get_block_size(); //???????????????????????????
                            if(descriptor->length < current_block * _io.get_block_size()){
                                descriptor->length = current_block * _io.get_block_size();
                                changed = true;
                            }
                            
                            if (changed){
                                save_descriptor(ofte->get_descriptor_index(),descriptor);
                            }
                            return NOSPACE;
                        }
                    }
                } else {
                    ofte->current_pos = descriptor->length;
                    if(descriptor->length < constraints::max_blocks_per_file * _io.get_block_size()){
                        descriptor->length = constraints::max_blocks_per_file * _io.get_block_size();                        
                        changed = true;
                    }
                    if (changed){
                        save_descriptor(ofte->get_descriptor_index(),descriptor);
                    }
                    return TOOBIG;
                }
            }      
        }
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
        std::uint8_t block_i = 1 + offset / _io.get_block_size();
        if (block_i >= constraints::descriptive_blocks_no) {
            return nullptr;
        }

        utils::disk_view dv{_io,  block_i, false};
        if (dv.block_i() == constraints::descriptive_blocks_no - 1 && offset % _io.get_block_size() > _io.get_block_size() - constraints::bytes_for_descriptor) {
            return nullptr;
        }

        std::size_t length = 0;
        std::array<std::size_t, constraints::max_blocks_per_file> occupied_blocks{};
        for (unsigned i = 0; i < constraints::bytes_for_file_length; i++, offset++) {
            length <<= 8;
            length += std::to_integer<std::size_t>(dv[offset]);
        }
        for (unsigned i = 0; i < constraints::max_blocks_per_file; i++, offset++) {
            occupied_blocks[i] = std::to_integer<std::size_t>(dv[offset]);
        }

        if (!(length == 0 && std::all_of(occupied_blocks.begin(), occupied_blocks.end(),[](const auto &value) { return value == 0; }))) {
            auto fd = new file_descriptor(length, occupied_blocks);
            _descriptors_cache[index] = fd;
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

} //namespace lab_fs
