#pragma once

#include <vector>
#include <array>
#include <map>
#include <string>
#include <utility>
#include <cstddef>

namespace lab_fs {

    class io {
    public:
        io(std::size_t blocks_no, std::size_t block_size, std::vector<std::vector<std::byte>> &&disk);

        void read_block(std::size_t i, std::vector<std::byte>::iterator dest);

        void write_block(std::size_t i, std::vector<std::byte>::iterator src);

        [[nodiscard]] std::size_t get_blocks_no() const;

        [[nodiscard]] std::size_t get_block_size() const;

    private:
        std::size_t blocks_no;
        std::size_t block_size;

        std::vector<std::vector<std::byte>> ldisk;
    };

    class file_system {
    private:
        class file_descriptor {
        public:
            file_descriptor(std::size_t length, std::initializer_list<std::size_t> occupied_blocks);

            std::size_t length;
            std::array<std::size_t, 3> occupied_blocks;
        };

        class oft_entry {
        public:
            oft_entry(std::string filename, std::size_t descriptor_index);

            [[nodiscard]] std::size_t get_descriptor_index() const;
            [[nodiscard]] std::string get_filename() const;

            std::vector<std::byte> buffer;
            std::size_t current_pos;
            bool modified;
        private:
            std::size_t descriptor_index;
            std::string filename;
        };

        std::string filename;
        io disk_io;
        std::vector<bool> available_blocks;
        std::vector<oft_entry *> oft;
        std::map<std::size_t, file_descriptor *> descriptors_map;
        std::map<std::string, std::size_t> descriptor_indexes_map;

        static constexpr std::size_t oft_max_size = 16;


        file_descriptor *get_descriptor(std::size_t i); //todo: implement
        void *write_descriptor(std::size_t i, file_descriptor *descriptor); //todo: implement
        void *remove_dir_entry(std::size_t i); //todo: implement

    public:
        file_system(std::string filename, io &&disk_io);

        //todo: Declare here create, destroy, open, close, read, write, seek, directory...

        void save();
    };

    enum init_result {
        CREATED, RESTORED, FAILED
    };

    std::pair<file_system *, init_result> init(std::size_t cylinders_no,
                                               std::size_t surfaces_no,
                                               std::size_t sections_no,
                                               std::size_t section_length,
                                               const std::string &filename);


} //namespace lab_fs