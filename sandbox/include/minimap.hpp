#pragma once

#include <string>
#include <vector>
#include <array>
#include <optional>
#include "assets.hpp"
#include "map.hpp"

namespace sandbox {

struct Quad {
    std::string texture;
    std::array<float, 4> rect; // x, y, width, height
    std::array<float, 4> uv;   // u, v, w, h
};

class Minimap {
public:
    std::vector<std::pair<std::string, Image>> images;
    std::array<float, 2> size;
    bool single;
    bool fitted;
    bool expanded;
    std::optional<size_t> pressed;

    static std::optional<Minimap> load(Assets& assets, uint32_t map_id);

    bool pointer(std::array<float, 2> position, bool down, std::array<float, 2> viewport);

    std::vector<Quad> quads(const Map& map, std::array<float, 2> player, std::array<float, 2> camera, std::array<float, 2> viewport) const;
};

} // namespace sandbox
