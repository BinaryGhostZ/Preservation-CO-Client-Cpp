#pragma once

#include "common.hpp"
#include <string>
#include <vector>
#include <span>

namespace sandbox_data {

struct MapSceneCell {
    uint32_t mask;
    int32_t terrain;
    int32_t altitude;
};

struct MapScenePart {
    VirtualPath ani_path;
    std::string section;
    int32_t offset_x;
    int32_t offset_y;
    uint32_t frame_interval_ms;
    uint32_t width;
    uint32_t height;
    int32_t thickness;
    int32_t scene_offset_x;
    int32_t scene_offset_y;
    int32_t elevation;
    std::vector<MapSceneCell> cells;
};

struct MapScene {
    std::vector<MapScenePart> parts;

    static MapScene parse(std::span<const uint8_t> bytes);
};

} // namespace sandbox_data
