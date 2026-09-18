#include "tq_archive_hash.hpp"
#include "common.hpp"
#include <vector>
#include <bit>
#include <algorithm>
#include <cstdint>

namespace sandbox_data {

constexpr size_t MAXIMUM_PATH_BYTES = 256;

uint32_t path_id(std::string_view normalized_path, const char* context) {
    auto is_ascii = [](std::string_view str) {
        return std::all_of(str.begin(), str.end(), [](unsigned char c) { return c < 128; });
    };

    if (!is_ascii(normalized_path) || normalized_path.length() > MAXIMUM_PATH_BYTES) {
        throw ContentError(context, "paths must be ASCII and no longer than 256 bytes");
    }

    size_t word_count = (normalized_path.length() + 3) / 4 + 2;
    std::vector<uint32_t> words(word_count, 0);

    for (size_t index = 0; index < normalized_path.length(); ++index) {
        words[index / 4] |= static_cast<uint32_t>(static_cast<unsigned char>(normalized_path[index])) << ((index % 4) * 8);
    }

    size_t suffix = words.size() - 2;
    words[suffix] = 0x9BE74448;
    words[suffix + 1] = 0x66F42C48;

    uint32_t value = 0xF4FA8928;
    uint32_t left = 0x7758B42B;
    uint32_t right = 0x37A8470E;

    for (uint32_t word : words) {
        value = std::rotl(value, 1);
        uint32_t mix = 0x267B0B11 ^ value;
        right ^= word;
        left ^= word;

        uint32_t first_factor = (mix + left | 0x02040801) & 0xBFEF7FDF;
        uint64_t product = static_cast<uint64_t>(first_factor) * static_cast<uint64_t>(right);
        
        uint32_t low = static_cast<uint32_t>(product);
        uint32_t high = static_cast<uint32_t>(product >> 32);
        if (high != 0) {
            low += 1;
        }
        
        uint64_t folded = static_cast<uint64_t>(low) + static_cast<uint64_t>(high);
        low = static_cast<uint32_t>(folded);
        if ((folded >> 32) != 0) {
            low += 1;
        }
        uint32_t next_right = low;

        uint32_t second_factor = (mix + right | 0x00804021) & 0x7DFEFBFF;
        right = next_right;
        
        product = static_cast<uint64_t>(left) * static_cast<uint64_t>(second_factor);
        low = static_cast<uint32_t>(product);
        high = static_cast<uint32_t>(product >> 32);
        
        folded = static_cast<uint64_t>(high) + static_cast<uint64_t>(high);
        uint32_t doubled_high = static_cast<uint32_t>(folded);
        if ((folded >> 32) != 0) {
            low += 1;
        }
        
        folded = static_cast<uint64_t>(low) + static_cast<uint64_t>(doubled_high);
        low = static_cast<uint32_t>(folded);
        if ((folded >> 32) != 0) {
            low += 2;
        }
        left = low;
    }

    return right ^ left;
}

} // namespace sandbox_data
