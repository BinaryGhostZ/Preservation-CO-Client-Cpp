#pragma once

#include "common.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <cstring>
#include <bit>

namespace sandbox_data {

class SliceReader {
    const char* context_;
    std::span<const uint8_t> bytes_;
    size_t offset_;

public:
    SliceReader(const char* context, std::span<const uint8_t> bytes)
        : context_(context), bytes_(bytes), offset_(0) {}

    size_t offset() const { return offset_; }
    size_t remaining() const { return bytes_.size() - offset_; }

    std::span<const uint8_t> read_exact(size_t length) {
        if (offset_ + length > bytes_.size() || offset_ + length < offset_) {
            throw ContentError(context_, "Truncated: needed " + std::to_string(length) + 
                                         " bytes at offset " + std::to_string(offset_));
        }
        auto result = bytes_.subspan(offset_, length);
        offset_ += length;
        return result;
    }

    void rewind(size_t length) {
        if (offset_ < length) {
            throw ContentError(context_, "Cannot rewind " + std::to_string(length) + 
                                         " bytes from offset " + std::to_string(offset_));
        }
        offset_ -= length;
    }

    void skip(ptrdiff_t length) {
        if (length > 0) {
            read_exact(static_cast<size_t>(length));
        } else if (length < 0) {
            rewind(static_cast<size_t>(-length));
        }
    }

    uint16_t read_u16_le() {
        auto span = read_exact(sizeof(uint16_t));
        uint16_t value;
        std::memcpy(&value, span.data(), sizeof(value));
        if constexpr (std::endian::native != std::endian::little) {
            value = std::byteswap(value);
        }
        return value;
    }

    uint32_t read_u32_le() {
        auto span = read_exact(sizeof(uint32_t));
        uint32_t value;
        std::memcpy(&value, span.data(), sizeof(value));
        if constexpr (std::endian::native != std::endian::little) {
            value = std::byteswap(value);
        }
        return value;
    }

    int16_t read_i16_le() {
        auto span = read_exact(sizeof(int16_t));
        int16_t value;
        std::memcpy(&value, span.data(), sizeof(value));
        if constexpr (std::endian::native != std::endian::little) {
            value = std::bit_cast<int16_t>(std::byteswap(std::bit_cast<uint16_t>(value)));
        }
        return value;
    }

    int32_t read_i32_le() {
        auto span = read_exact(sizeof(int32_t));
        int32_t value;
        std::memcpy(&value, span.data(), sizeof(value));
        if constexpr (std::endian::native != std::endian::little) {
            // byteswap technically takes unsigned, but we can cast
            value = std::bit_cast<int32_t>(std::byteswap(std::bit_cast<uint32_t>(value)));
        }
        return value;
    }

    float read_f32_le() {
        auto span = read_exact(sizeof(float));
        uint32_t value;
        std::memcpy(&value, span.data(), sizeof(value));
        if constexpr (std::endian::native != std::endian::little) {
            value = std::byteswap(value);
        }
        return std::bit_cast<float>(value);
    }
};

} // namespace sandbox_data
