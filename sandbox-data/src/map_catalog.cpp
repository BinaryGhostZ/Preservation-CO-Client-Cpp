#include "map_catalog.hpp"
#include "reader.hpp"
#include <algorithm>

namespace sandbox_data {

GameMapCatalog GameMapCatalog::parse(std::span<const uint8_t> bytes) {
    SliceReader reader("GameMap.dat", bytes);
    uint32_t count = reader.read_u32_le();
    
    if (count > MAXIMUM_MAP_RECORDS) {
        throw ContentError("GameMap.dat", "map record count is unreasonable");
    }

    GameMapCatalog catalog;
    catalog.records.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t id = reader.read_u32_le();
        uint32_t path_length = reader.read_u32_le();
        
        if (path_length == 0 || path_length > 512) {
            throw ContentError("GameMap.dat", "invalid GameMap.dat path length");
        }
        
        auto raw_path = reader.read_exact(path_length);
        size_t path_end = 0;
        for (; path_end < raw_path.size(); ++path_end) {
            if (raw_path[path_end] == '\0') break;
        }
        
        std::string path(reinterpret_cast<const char*>(raw_path.data()), path_end);
        std::replace(path.begin(), path.end(), '\\', '/');
        
        uint32_t tile_size = reader.read_u32_le();
        if (tile_size == 0 || tile_size > 4096) {
            throw ContentError("GameMap.dat", "invalid tile size for map");
        }
        
        GameMapRecord record;
        record.id = id;
        record.dmap_path = path;
        record.tile_size = tile_size;
        
        if (catalog.records.find(id) != catalog.records.end()) {
            throw ContentError("GameMap.dat", "map appears more than once");
        }
        catalog.records[id] = record;
    }
    
    if (reader.remaining() != 0) {
        throw ContentError("GameMap.dat", "GameMap.dat has unexplained trailing bytes");
    }
    
    return catalog;
}

const GameMapRecord* GameMapCatalog::get(uint32_t id) const {
    auto it = records.find(id);
    if (it != records.end()) {
        return &it->second;
    }
    return nullptr;
}

void GameMapCatalog::merge_distinct(const GameMapCatalog& extension) {
    for (const auto& [id, record] : extension.records) {
        if (records.find(id) != records.end()) {
            throw ContentError("GameMap.dat", "extension map conflicts with an existing map record");
        }
        records[id] = record;
    }
}

} // namespace sandbox_data
