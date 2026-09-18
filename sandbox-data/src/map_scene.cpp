#include "map_scene.hpp"
#include "reader.hpp"
#include <algorithm>

namespace sandbox_data {

static std::string fixed_string(SliceReader& reader, size_t length) {
    auto span = reader.read_exact(length);
    size_t end = 0;
    for (; end < span.size(); ++end) {
        if (span[end] == '\0') break;
    }
    std::string str(reinterpret_cast<const char*>(span.data()), end);
    std::replace(str.begin(), str.end(), '\\', '/');
    return str;
}

MapScene MapScene::parse(std::span<const uint8_t> bytes) {
    SliceReader reader("map scene", bytes);
    
    int32_t raw_part_count = reader.read_i32_le();
    if (raw_part_count < 0 || raw_part_count > MAXIMUM_MAP_OBJECTS) {
        throw ContentError("map scene", "invalid scene part count");
    }
    uint32_t part_count = static_cast<uint32_t>(raw_part_count);
    
    std::vector<MapScenePart> parts;
    parts.reserve(part_count);
    
    for (uint32_t i = 0; i < part_count; ++i) {
        std::string ani = fixed_string(reader, 256);
        std::string section = fixed_string(reader, 64);
        
        int32_t offset_x = reader.read_i32_le();
        int32_t offset_y = reader.read_i32_le();
        uint32_t frame_interval_ms = reader.read_u32_le();
        uint32_t width = reader.read_u32_le();
        uint32_t height = reader.read_u32_le();
        
        uint64_t cell_count_64 = static_cast<uint64_t>(width) * height;
        if (cell_count_64 > 10000000) {
            throw ContentError("map scene part", "area too large");
        }
        uint32_t cell_count = static_cast<uint32_t>(cell_count_64);
        
        int32_t thickness = reader.read_i32_le();
        int32_t scene_offset_x = reader.read_i32_le();
        int32_t scene_offset_y = reader.read_i32_le();
        int32_t elevation = reader.read_i32_le();
        
        std::vector<MapSceneCell> cells;
        cells.reserve(cell_count);
        for (uint32_t j = 0; j < cell_count; ++j) {
            MapSceneCell cell;
            cell.mask = reader.read_u32_le();
            cell.terrain = reader.read_i32_le();
            cell.altitude = reader.read_i32_le();
            cells.push_back(cell);
        }
        
        MapScenePart part;
        part.ani_path = ani;
        part.section = section;
        part.offset_x = offset_x;
        part.offset_y = offset_y;
        part.frame_interval_ms = frame_interval_ms;
        part.width = width;
        part.height = height;
        part.thickness = thickness;
        part.scene_offset_x = scene_offset_x;
        part.scene_offset_y = scene_offset_y;
        part.elevation = elevation;
        part.cells = std::move(cells);
        
        parts.push_back(std::move(part));
    }
    
    if (reader.remaining() != 0) {
        throw ContentError("map scene", "trailing bytes");
    }
    
    MapScene scene;
    scene.parts = std::move(parts);
    return scene;
}

} // namespace sandbox_data
