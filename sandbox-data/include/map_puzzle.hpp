#pragma once

#include "common.hpp"
#include <string>
#include <vector>
#include <array>
#include <span>

namespace sandbox_data {

struct PuzzleTextureGroup {
    int16_t tile_id;
    std::vector<uint8_t> label;
    VirtualPath ani_path;
    std::string section;
    std::array<uint32_t, 5> parameters;
};

struct PuzzleCellLayer {
    int16_t tile_id;
    uint32_t coverage_mask;
};

struct PuzzleEdgeRecord {
    uint32_t group;
    std::array<uint8_t, 4> sides;
};

struct PuzzleLayout {
    std::string kind;
    VirtualPath ani_path;
    uint32_t columns;
    uint32_t rows;
    std::vector<int16_t> tiles;

    std::vector<std::vector<PuzzleCellLayer>> cell_layers;
    std::vector<PuzzleTextureGroup> texture_groups;
    std::vector<PuzzleTextureGroup> edge_groups;
    std::vector<PuzzleEdgeRecord> edge_records;

    std::array<int32_t, 2> roll_speed;
    std::vector<uint8_t> trailing_bytes;

    bool is_layered() const {
        return !cell_layers.empty();
    }

    static PuzzleLayout parse(std::span<const uint8_t> bytes);
};

bool is_supported_5065_pux_material(const std::array<uint32_t, 5>& parameters);

} // namespace sandbox_data
