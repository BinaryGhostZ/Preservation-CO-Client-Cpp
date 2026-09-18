#pragma once

#include "common.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <optional>
#include <span>
#include "map_scene.hpp"

namespace sandbox_data {

struct MapCell {
    int16_t access;
    int16_t surface;
    int16_t elevation;
};

struct MapPortal {
    int32_t x;
    int32_t y;
    int32_t index;
};

struct TerrainObject {
    VirtualPath ani_path;
    std::string section;
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    int32_t offset_x;
    int32_t offset_y;
    uint32_t frame_interval_ms;
    
    bool is_renderable() const {
        return width > 0 && height > 0;
    }
};

struct MapSceneObject {
    VirtualPath scene_path;
    int32_t x;
    int32_t y;
};

struct MapEffectObject {
    std::string effect;
    int32_t world_x;
    int32_t world_y;
};

struct MapSoundObject {
    VirtualPath sound_path;
    int32_t world_x;
    int32_t world_y;
    uint32_t range;
    uint32_t volume;
    uint32_t interval_ms;
};

struct MapTransformedEffectObject {
    std::string effect;
    int32_t world_x;
    int32_t world_y;
    uint32_t transform_bits[6];
};

struct MapAdditionalPuzzleLayer {
    uint32_t insertion_index;
    int32_t move_rate_x;
    int32_t move_rate_y;
    uint32_t state_words[3];
    std::vector<VirtualPath> puzzle_paths;
};

struct DMap {
    uint32_t header[2];
    VirtualPath puzzle_path;
    uint32_t width;
    uint32_t height;
    std::vector<MapCell> cells;
    std::vector<uint32_t> row_checksums;
    std::vector<MapPortal> portals;
    std::vector<MapSceneObject> scene_objects;
    std::vector<TerrainObject> terrain_objects;
    std::vector<MapEffectObject> effect_objects;
    std::vector<MapSoundObject> sound_objects;
    std::vector<MapTransformedEffectObject> transformed_effect_objects;

    std::optional<uint32_t> additional_layer_prefix;
    std::vector<MapAdditionalPuzzleLayer> additional_puzzle_layers;
    std::vector<uint8_t> trailing_bytes;

    static DMap parse(std::span<const uint8_t> bytes);
    
    std::optional<MapCell> cell(uint32_t x, uint32_t y) const {
        if (x >= width || y >= height) return std::nullopt;
        size_t index = static_cast<size_t>(y) * width + x;
        if (index < cells.size()) {
            return cells[index];
        }
        return std::nullopt;
    }
};

void apply_fixed_scene_layers(DMap& dmap, int32_t scene_x, int32_t scene_y, const MapScene& scene);

} // namespace sandbox_data
