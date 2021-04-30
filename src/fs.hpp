#pragma once

#include <io.hpp>

#include <vector>
#include <array>
#include <map>
#include <string>
#include <utility>
#include <cstddef>

namespace lab_fs {

    enum init_result {
        CREATED, RESTORED, FAILED
    };
    enum fs_result {
        SUCCESS, EXISTS, NO_SPACE, NOT_FOUND, TOO_BIG, INVALID_NAME, INVALID_POS, ALREADY_OPENED
    };

    class file_system {
    public:
        struct constraints {
            static constexpr std::size_t descriptive_blocks_no = 2;
            static constexpr std::size_t bytes_for_file_length = 2;
            static constexpr std::size_t max_blocks_per_file = 3;
            static constexpr std::size_t max_filename_length = 15;
            static constexpr std::size_t oft_max_size = 16;
            static constexpr std::size_t bytes_for_descriptor = bytes_for_file_length + max_blocks_per_file;          
            
            constraints() = delete;
        };
        const std::size_t max_files_quantity = constraints::max_blocks_per_file * _io.get_block_size() / (constraints::max_filename_length + 1);

    private:
        class file_descriptor {
        public:
            file_descriptor(std::size_t length,
                            const std::array<std::size_t, constraints::max_blocks_per_file> &occupied_blocks);
            bool is_initialized() const;

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
            std::size_t current_rel_block; // current relative block - index in block array of descriptor
            bool modified;
            bool initialized;
        private:
            std::size_t _descriptor_index;
            std::string _filename;
        };

        std::string _filename;
        io _io;
        std::vector<bool> _bitmap;
        std::vector<oft_entry *> _oft;
        std::map<std::size_t, file_descriptor *> _descriptors_cache; // (index of desc) -> (file desc)
        std::map<std::string, std::size_t> _descriptor_indexes_cache; // (_filename) -> (index of desc)

        auto get_descriptor(std::size_t index, bool disable_caching = false) -> file_descriptor *;
        auto save_descriptor(std::size_t index, file_descriptor *descriptor) -> bool;
        auto take_descriptor() -> int;

        auto get_descriptor_index_from_dir_entry(const std::string& filename) -> int;
        auto take_dir_entry(const std::string& filename) -> std::pair<std::size_t, fs_result>;
        auto save_dir_entry(std::size_t i, std::string filename, std::size_t descriptor_index) -> bool;
        auto allocate_block(file_descriptor *descriptor, std::size_t block_index) -> bool;

        auto initialize_oft_entry(oft_entry* entry, std::size_t block) -> fs_result;
        auto initialize_file_descriptor(file_descriptor* descriptor, std::size_t block) -> fs_result;

    public:
        file_system(std::string filename, io &&disk_io);

        static std::pair<file_system *, init_result> init(std::size_t cylinders_no,
                                                          std::size_t surfaces_no,
                                                          std::size_t sections_no,
                                                          std::size_t section_length,
                                                          const std::string &filename);

        void save(const std::string &filename);
        void save();

        auto lseek(std::size_t i, std::size_t pos) -> fs_result;
        auto create(const std::string& filename) -> fs_result;
        auto open(const std::string& filename) -> std::pair<std::size_t, fs_result>;
        auto write(std::size_t i, const std::vector<std::byte>& src) -> fs_result;
        auto read(std::size_t i, std::vector<std::byte>::iterator mem_area, std::size_t count) -> fs_result;
        auto close(std::size_t i) -> fs_result;
        auto directory() -> std::vector<std::pair<std::string, std::size_t>>;

        //todo: Declare here destroy, close, read, directory...
    };

} //namespace lab_fs
