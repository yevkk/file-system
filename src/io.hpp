#pragma once

#include <vector>
#include <cassert>

namespace lab_fs {
    class io {
    public:
        io(std::size_t blocks_no, std::size_t block_size, std::vector<std::vector<std::byte>> &&disk) :
                _blocks_no{blocks_no},
                _block_size{block_size},
                _ldisk{disk} {}

        void read_block(std::size_t i, std::vector<std::byte>::iterator dest) {
            assert(i < _blocks_no);
            std::copy(_ldisk[i].cbegin(), _ldisk[i].cend(), dest);
        }

        void write_block(std::size_t i, std::vector<std::byte>::iterator src) {
            assert(i < _blocks_no);
            std::copy(src, src + (int) _block_size, _ldisk[i].begin());
        }

        [[nodiscard]] std::size_t get_blocks_no() const {
            return _blocks_no;
        }

        [[nodiscard]] std::size_t get_block_size() const {
            return _block_size;
        }

    private:
        std::size_t _blocks_no;
        std::size_t _block_size;

        std::vector<std::vector<std::byte>> _ldisk;
    };

} //namespace lab_fs