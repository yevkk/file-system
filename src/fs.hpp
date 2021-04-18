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
    public:
        struct constraints {
            static constexpr std::size_t descriptive_blocks_no = 2;
            static constexpr std::size_t bytes_for_file_length = 2;
            static constexpr std::size_t max_blocks_per_file = 3;
            static constexpr std::size_t max_filename_length = 15;
            static constexpr std::size_t oft_max_size = 16;
        };
    private:
        class file_descriptor {
        public:
            file_descriptor(std::size_t length, const std::array<std::size_t, constraints::max_blocks_per_file> &occupied_blocks);

            std::size_t length;
            std::array<std::size_t, constraints::max_blocks_per_file> occupied_blocks;
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
        std::map<std::size_t, file_descriptor *> descriptors_cache; // index of desc && file desc
        std::map<std::string, std::size_t> descriptor_indexes_cache; //filename && index of desc

        file_descriptor *get_descriptor(std::size_t i); //todo: implement
        void *write_descriptor(std::size_t i, file_descriptor *descriptor); //todo: implement
        void *remove_dir_entry(std::size_t i); //todo: implement
        bool write_dir_entry(std::string filename, std::size_t descriptor_index);
        std::size_t get_dir_entry(std::string filename); // -1 if there's none

    public:
        file_system(std::string filename, io &&disk_io);

        //todo: Declare here create, destroy, open, close, read, write, seek, directory...
        fs_result create(const std::string& filename);
        std::pair<std::size_t, fs_result> open(const std::string& filename);
        fs_result write(std::size_t i, const std::vector<std::byte>& src);

        void save();
    };

    enum init_result {
        CREATED, RESTORED, FAILED
    };

    enum fs_result {
        SUCCESS, EXISTS, NOSPACE, NOTFOUND
    };

    std::pair<file_system *, init_result> init(std::size_t cylinders_no,
                                               std::size_t surfaces_no,
                                               std::size_t sections_no,
                                               std::size_t section_length,
                                               const std::string &filename);


} //namespace lab_fs