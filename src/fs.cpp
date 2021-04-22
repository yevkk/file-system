#include "fs.hpp"

#include <fstream>
#include <cassert>

namespace lab_fs {

    file_system::file_descriptor::file_descriptor(std::size_t length,
                                                  const std::array<std::size_t, constraints::max_blocks_per_file> &occupied_blocks)
            :
            length{length},
            occupied_blocks{occupied_blocks} {}

    bool file_system::file_descriptor::is_initialized() const {
        if (length > 0)
            return true;        
        for(auto i: occupied_blocks){
            if(i != 255) {
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
        class dir_entry {
        public:
            static constexpr std::size_t dir_entry_size = file_system::constraints::max_filename_length + 1;
        public:
            dir_entry(const dir_entry& d) = default;

            dir_entry(const std::string& filename, std::byte descriptor_index) :                   
                    filename{filename}, descriptor_index{descriptor_index}  {}

            dir_entry(std::vector<std::byte>& container) {
                filename = "";
                for(int i = 0; i < file_system::constraints::max_filename_length; ++i){
                    if(container[i] == std::byte{0}){
                        break;
                    }
                    filename.push_back(char(container[i]));
                }
                descriptor_index = container[dir_entry_size-1];
            }

            bool is_empty() const {
                return (filename == "") && (descriptor_index == std::byte{0});
            }

            std::vector<std::byte> convert() {
                auto container = std::vector<std::byte>(dir_entry_size);
                for(int i = 0; i < filename.size() && i < file_system::constraints::max_filename_length; ++i){
                    container[i] = std::byte{filename[i]};
                }
                for(int i = filename.size(); i < file_system::constraints::max_filename_length; ++i){
                    container[i] = std::byte{0};
                }
                container[dir_entry_size-1] = descriptor_index;
                return container;
            }

            static std::optional<dir_entry> read_dir_entry(file_system* fs ,std::size_t i) {                               
                std::size_t pos = i * (dir_entry_size);
                if(fs->lseek(0, pos) == SUCCESS) {
                    auto container = std::vector<std::byte>(dir_entry_size);
                    
                    // TODO: replace with (fs->read(0, container) == SUCCESS) or something like that
                    if(true){
                        return std::optional<dir_entry>{dir_entry(container)};
                    } else {
                        return std::nullopt;;
                    }
                } else {
                    return std::nullopt;
                }
            }

        public:
            std::string filename;
            std::byte descriptor_index; 
        };

        
    }

    int file_system::get_descriptor_index_from_dir_entry(const std::string& filename){
        std::size_t i = 0;
        while (true){
            auto dire_opt = utils::dir_entry::read_dir_entry(this,i);
            if (!dire_opt.has_value()){
                return -1;
            }
            auto dire = dire_opt.value();
            if (dire.filename == filename){
                return int(dire.descriptor_index);
            }
            ++i;
        }
    }

    // picks last free space and reads through all to verify there is no same file
    std::pair<std::size_t, fs_result> file_system::take_dir_entry(const std::string& filename) {
        if (!_oft[0]->initialized){
            return {0, SUCCESS};
        }
        
        std::size_t i = 0;
        int free = -1;
        while (true){
            auto dire_opt = utils::dir_entry::read_dir_entry(this,i);
            
            // looked through all dir entries and none of them is free
            if (!dire_opt.has_value()){
                if (free == -1){
                    return {0, NO_SPACE};
                }
                return {free, SUCCESS};
            }
            
            auto dire = dire_opt.value();
            
            // check if file has same name
            if(dire.filename == filename){
                return {0, EXISTS};
            }

            // remember empty slot
            if (dire.is_empty()){
                free = i;
            }
            ++i;
        }        
    }  

    bool file_system::save_dir_entry(std::size_t i, std::string filename, std::size_t descriptor_index){       
        std::size_t pos = i * (sizeof(utils::dir_entry));
        if(lseek(0, pos) == SUCCESS) {
            auto data = utils::dir_entry{filename,std::byte{descriptor_index}}.convert();
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
            return INVALID_NAME;
        }

        auto result = take_dir_entry(filename);
        if (result.second != SUCCESS){
            return result.second;
        }
        
        auto index = result.first;
        auto descriptor_index = take_descriptor();
        if(descriptor_index == -1)
            return NO_SPACE;

        save_dir_entry(index,filename,descriptor_index);
        return SUCCESS;
    }

    std::pair<std::size_t, fs_result> file_system::open(const std::string& filename){       
        if(filename.size() > constraints::max_filename_length){
            return {0,INVALID_NAME};
        }
        
        if (_oft.size() == constraints::oft_max_size){
            return {0,NO_SPACE};
        }
        
        int index;
        
        // check if file info is already cached
        if(_descriptor_indexes_cache.contains(filename)){
            index = _descriptor_indexes_cache[filename];
        } else {
            index = get_descriptor_index_from_dir_entry(filename);
            if(index == -1)
                return {0,NOT_FOUND};          
        }
        get_descriptor(index);      
        _oft.emplace_back(new oft_entry{filename,index});    
        return {_oft.size()-1,SUCCESS};
    }

    bool file_system::allocate_block(file_descriptor *descriptor, std::size_t block_index){
        for(int i = constraints::descriptive_blocks_no; i < _io.get_blocks_no(); ++i){
            if (!_bitmap[i]){
                _bitmap[i] = true;
                descriptor->occupied_blocks[block_index] = i;
                return true;
            }
        }
        return false;
    }

    fs_result file_system::write(std::size_t i, const std::vector<std::byte>& src){
        if (i > _oft.size()) {
            return NOT_FOUND;
        }        
        if (src.size() == 0) {
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
        if (!ofte->initialized){
            if(descriptor->is_initialized()){
                _io.read_block(descriptor->occupied_blocks[current_block],buffer.begin());
            } else {
                if (allocate_block(descriptor, 0)){
                    for(int i = 1; i < constraints::max_blocks_per_file; ++i){
                        descriptor->occupied_blocks[i] = 0;
                    }
                    changed = true;
                } else {
                    return NO_SPACE;
                }
            }
            ofte->initialized = true;
            buffer = std::vector<std::byte>(_io.get_block_size(), std::byte{0});
        }

        while(true){
            // fits within current block
            if (src.size() - offset <= _io.get_block_size() - pos) {
                std::copy(src.begin()+offset, src.end(), buffer.begin()+pos);
                ofte->modified = true;
                new_pos = pos + src.size() - offset;
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
                auto part = _io.get_block_size() - pos;
                std::copy(src.begin()+offset, src.begin()+offset+part, buffer.begin()+pos);               
                offset += part;
                new_pos = pos + part;

                //save changes to disk
                _io.write_block(descriptor->occupied_blocks[current_block],buffer.begin());               
                ofte->modified = false;

                // check if there is space to continue
                if(current_block < constraints::max_blocks_per_file - 1){
                    ++current_block;
                    // try to read next allocated block
                    if (descriptor->occupied_blocks[current_block] != 0){
                        _io.read_block(descriptor->occupied_blocks[current_block],buffer.begin());                       
                        pos = 0;
                    }
                    // try to allocate new block
                    else {
                        if(allocate_block(descriptor,current_block)){
                            changed = true;
                            std::fill(buffer.begin(),buffer.end(),std::byte(0));
                        } else {
                            ofte->current_pos = current_block * _io.get_block_size();
                            if(descriptor->length < current_block * _io.get_block_size()){
                                descriptor->length = current_block * _io.get_block_size();
                                changed = true;
                            }
                           
                            if (changed){
                                save_descriptor(ofte->get_descriptor_index(),descriptor);
                            }
                            return NO_SPACE;
                        }
                    }
                } 
                // file has reached the max size
                else {                    
                    if(descriptor->length < constraints::max_blocks_per_file * _io.get_block_size()){
                        descriptor->length = constraints::max_blocks_per_file * _io.get_block_size();                        
                        changed = true;
                    }
                    ofte->current_pos = descriptor->length;
                    if (changed){
                        save_descriptor(ofte->get_descriptor_index(),descriptor);
                    }
                    return TOO_BIG;
                }
            }      
        }
    }

    fs_result file_system::lseek(std::size_t i, std::size_t pos){
        if (i > _oft.size()) {
            return NOT_FOUND;
        }
        
        auto ofte = _oft[i];
        auto descriptor=  _descriptors_cache[ofte->get_descriptor_index()];       
        if (pos > descriptor->length)
        {
            return INVALID_POS;
        }
        
        std::size_t current_block = ofte->current_pos / _io.get_block_size();
        std::size_t new_block = pos / _io.get_block_size();
        if(current_block != new_block && ofte->modified) {
            _io.write_block(current_block, ofte->buffer.begin());
            _io.read_block(new_block,ofte->buffer.begin());
            ofte->modified = false;
        }
        ofte->current_pos = pos;
        return SUCCESS;
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
            disk[0][0] = std::byte{192}; // 192 = 11000000

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
