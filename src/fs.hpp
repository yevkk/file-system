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
            std::size_t _descriptor_index;
            std::string _filename;
        };

        std::string _filename;
        io _io;
        std::vector<bool> _bitmap;
        std::vector<oft_entry *> _oft;
        std::map<std::size_t, file_descriptor *> _descriptors_cache; // index of desc && file desc
        std::map<std::string, std::size_t> _descriptor_indexes_cache; //_filename && index of desc

        file_descriptor *get_descriptor(std::size_t descriptor_index); //todo: implement
        void *save_descriptor(std::size_t i, file_descriptor *descriptor); //todo: implement
        int take_descriptor(); //todo: implement

    public:
        file_system(std::string filename, io &&disk_io);

        static std::pair<file_system *, init_result> init(std::size_t cylinders_no,
                                                          std::size_t surfaces_no,
                                                          std::size_t sections_no,
                                                          std::size_t section_length,
                                                          const std::string &filename);

        void save();

        //todo: Declare here create, destroy, open, close, read, write, seek, directory...
    };

} //namespace lab_fs