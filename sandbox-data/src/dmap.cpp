#include "dmap.hpp"
#include "reader.hpp"
#include <stdexcept>
#include <algorithm>

namespace sandbox_data {

static std::string fixed_string(SliceReader& reader, size_t length) {
    auto span = reader.read_exact(length);
    size_t string_len = 0;
    while (string_len < length && span[string_len] != '\0') {
        string_len++;
    }
    return std::string(reinterpret_cast<const char*>(span.data()), string_len);
}

DMap DMap::parse(std::span<const uint8_t> bytes) {
    SliceReader reader("DMap", bytes);
    DMap map;
    
    map.header[0] = reader.read_u32_le();
    map.header[1] = reader.read_u32_le();
    
    map.puzzle_path = VirtualPath(fixed_string(reader, 260));
    map.width = reader.read_u32_le();
    map.height = reader.read_u32_le();
    
    size_t cell_count = static_cast<size_t>(map.width) * map.height;
    if (cell_count > 10000000) { // sanity check
        throw ContentError("DMap", "Map too large");
    }
    
    map.cells.reserve(cell_count);
    map.row_checksums.reserve(map.height);
    
    for (uint32_t y = 0; y < map.height; ++y) {
        for (uint32_t x = 0; x < map.width; ++x) {
            MapCell cell;
            cell.access = reader.read_i16_le();
            cell.surface = reader.read_i16_le();
            cell.elevation = reader.read_i16_le();
            map.cells.push_back(cell);
        }
        map.row_checksums.push_back(reader.read_u32_le());
    }

    int32_t portal_count = reader.read_i32_le();
    if (portal_count < 0 || portal_count > MAXIMUM_MAP_OBJECTS) throw ContentError("DMap", "Invalid portal count");
    map.portals.reserve(portal_count);
    for (int32_t i = 0; i < portal_count; ++i) {
        MapPortal portal;
        portal.x = reader.read_i32_le();
        portal.y = reader.read_i32_le();
        portal.index = reader.read_i32_le();
        map.portals.push_back(portal);
    }

    int32_t object_count = reader.read_i32_le();
    if (object_count < 0 || object_count > MAXIMUM_MAP_OBJECTS) throw ContentError("DMap", "Invalid object count");
    
    bool extended_object_layout = (map.header[0] == 1005);

    for (int32_t i = 0; i < object_count; ++i) {
        int32_t object_type = reader.read_i32_le();
        if (object_type == 0) {
            object_type = reader.read_i32_le();
        }

        switch (object_type) {
            case 1: {
                MapSceneObject obj;
                obj.scene_path = VirtualPath(fixed_string(reader, 260));
                obj.x = reader.read_i32_le();
                obj.y = reader.read_i32_le();
                map.scene_objects.push_back(obj);
                break;
            }
            case 4:
            case 24: {
                TerrainObject obj;
                obj.ani_path = VirtualPath(fixed_string(reader, 260));
                obj.section = fixed_string(reader, 128);
                obj.x = reader.read_i32_le();
                obj.y = reader.read_i32_le();
                obj.width = reader.read_i32_le();
                obj.height = reader.read_i32_le();
                obj.offset_x = reader.read_i32_le();
                obj.offset_y = reader.read_i32_le();
                obj.frame_interval_ms = reader.read_u32_le();
                map.terrain_objects.push_back(obj);
                break;
            }
            case 10: {
                MapEffectObject obj;
                obj.effect = fixed_string(reader, 64);
                obj.world_x = reader.read_i32_le();
                obj.world_y = reader.read_i32_le();
                map.effect_objects.push_back(obj);
                break;
            }
            case 19: {
                if (extended_object_layout) {
                    MapTransformedEffectObject obj;
                    obj.effect = fixed_string(reader, 64);
                    obj.world_x = reader.read_i32_le();
                    obj.world_y = reader.read_i32_le();
                    for (int j = 0; j < 6; ++j) {
                        obj.transform_bits[j] = reader.read_u32_le();
                    }
                    map.transformed_effect_objects.push_back(obj);
                } else {
                    reader.skip(72);
                }
                break;
            }
            case 15: {
                if (extended_object_layout) {
                    MapSoundObject obj;
                    obj.sound_path = VirtualPath(fixed_string(reader, 260));
                    obj.world_x = reader.read_i32_le();
                    obj.world_y = reader.read_i32_le();
                    obj.range = reader.read_u32_le();
                    obj.volume = reader.read_u32_le();
                    obj.interval_ms = reader.read_u32_le();
                    map.sound_objects.push_back(obj);
                } else {
                    reader.skip(276);
                }
                break;
            }
            default:
                throw ContentError("DMap", "Unsupported object type");
        }
    }

    if (extended_object_layout && reader.remaining() > 0) {
        if (reader.remaining() < 8) {
            throw ContentError("DMap", "Truncated additional layer");
        }
        map.additional_layer_prefix = reader.read_u32_le();
        uint32_t layer_count = reader.read_u32_le();
        if (layer_count > MAXIMUM_MAP_OBJECTS) throw ContentError("DMap", "Too many layers");

        for (uint32_t i = 0; i < layer_count; ++i) {
            MapAdditionalPuzzleLayer layer;
            layer.insertion_index = reader.read_u32_le();
            uint32_t layer_type = reader.read_u32_le();
            if (layer_type != 4) throw ContentError("DMap", "Unsupported layer type");
            
            layer.move_rate_x = reader.read_i32_le();
            layer.move_rate_y = reader.read_i32_le();
            for (int j = 0; j < 3; ++j) layer.state_words[j] = reader.read_u32_le();
            
            uint32_t obj_count = reader.read_u32_le();
            if (obj_count > MAXIMUM_MAP_OBJECTS) throw ContentError("DMap", "Too many layer objects");
            
            for (uint32_t j = 0; j < obj_count; ++j) {
                if (reader.read_u32_le() != 8) throw ContentError("DMap", "Unsupported layer obj");
                layer.puzzle_paths.push_back(VirtualPath(fixed_string(reader, 260)));
            }
            map.additional_puzzle_layers.push_back(layer);
        }
    }

    if (reader.remaining() > 0) {
        auto rem = reader.read_exact(reader.remaining());
        map.trailing_bytes.assign(rem.begin(), rem.end());
    }

    return map;
}

void apply_fixed_scene_layers(DMap& dmap, int32_t scene_x, int32_t scene_y, const MapScene& scene) {
    for (const auto& part : scene.parts) {
        int32_t part_x = scene_x + part.scene_offset_x;
        int32_t part_y = scene_y + part.scene_offset_y;
        
        for (uint32_t row = 0; row < part.height; ++row) {
            for (uint32_t column = 0; column < part.width; ++column) {
                size_t part_index = static_cast<size_t>(row) * part.width + column;
                if (part_index >= part.cells.size()) continue;
                
                const auto& layer = part.cells[part_index];
                
                int32_t x = part_x - static_cast<int32_t>(column);
                int32_t y = part_y - static_cast<int32_t>(row);
                
                if (x < 0 || y < 0) continue;
                
                uint32_t ux = static_cast<uint32_t>(x);
                uint32_t uy = static_cast<uint32_t>(y);
                
                if (ux >= dmap.width || uy >= dmap.height) continue;
                
                size_t index = static_cast<size_t>(uy) * dmap.width + ux;
                auto previous = dmap.cells[index];
                
                dmap.cells[index] = MapCell {
                    static_cast<int16_t>(layer.mask),
                    static_cast<int16_t>(layer.terrain),
                    static_cast<int16_t>(previous.elevation + layer.altitude)
                };
            }
        }
    }
}

} // namespace sandbox_data
