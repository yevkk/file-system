#pragma once

#include <cstdint>
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

    namespace utils {
        class disk_view {
        public:
            explicit disk_view(io &disk_io, std::uint8_t start_block = 0) :
                    _io{disk_io},
                    _block_i{start_block} {
                _buffer.resize(_io.get_block_size());
                _io.read_block(_block_i, _buffer.begin());
            }

            std::byte &operator [](std::size_t index) {
                std::size_t queried_block_i = index / _io.get_block_size();
                assert(queried_block_i < _io.get_blocks_no() && "Out of range");
                std::size_t local_index = index % _io.get_block_size();

                if (queried_block_i == _block_i) {
                    return _buffer[local_index];
                }

                if (queried_block_i == _block_i - 1 && _block_i != 0 && !_prev_buffer.empty()) {
                    return _prev_buffer[local_index];
                }

                if (!_prev_buffer.empty()) {
                    _io.write_block(_block_i - 1, _prev_buffer.begin());
                }

                if (queried_block_i == _block_i + 1) {
                    _prev_buffer = std::move(_buffer);
                    _buffer.resize(_io.get_block_size());
                } else {
                    _io.write_block(_block_i, _buffer.begin());
                    _prev_buffer.clear();
                }

                _block_i = queried_block_i;
                _io.read_block(_block_i, _buffer.begin());

                return _buffer[local_index];
            }

            void push_buffer() {
                if (!_buffer.empty()) {
                    _io.write_block(_block_i, _buffer.begin());
                }

                if (!_prev_buffer.empty()) {
                    _io.write_block(_block_i - 1, _prev_buffer.begin());
                }
            }

        private:
            io &_io;
            std::uint8_t _block_i;
            std::vector<std::byte> _buffer;
            std::vector<std::byte> _prev_buffer;
        };
    }

} //namespace lab_fs