#pragma once

#include "common.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <span>

namespace sandbox_data {

constexpr uint32_t MAXIMUM_MAP_RECORDS = 5000;

struct GameMapRecord {
    uint32_t id;
    VirtualPath dmap_path;
    uint32_t tile_size;
};

class GameMapCatalog {
public:
    std::unordered_map<uint32_t, GameMapRecord> records;

    static GameMapCatalog parse(std::span<const uint8_t> bytes);
    const GameMapRecord* get(uint32_t id) const;
    void merge_distinct(const GameMapCatalog& extension);
    
    bool is_empty() const { return records.empty(); }
    size_t len() const { return records.size(); }
};

} // namespace sandbox_data
