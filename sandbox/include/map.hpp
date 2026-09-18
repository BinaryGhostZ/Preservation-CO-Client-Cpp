#pragma once

#include "assets.hpp"
#include "dmap.hpp"
#include "map_catalog.hpp"
#include "map_puzzle.hpp"
#include "map_scene.hpp"
#include <string>
#include <vector>
#include <array>

namespace sandbox {

struct Sprite {
    std::vector<std::string> frames;
    float origin[2];
    float depth;
    uint32_t interval;
};

class Map {
public:
    uint32_t id;
    sandbox_data::DMap data;
    std::vector<Sprite> sprites;
    float extent[2];

    static Map load(Assets& assets, uint32_t id);
    
    std::array<float, 2> project(const std::array<float, 2>& p) const;
    std::array<float, 2> unproject(const std::array<float, 2>& p) const;
    bool walkable(const std::array<int32_t, 2>& p) const;
};

} // namespace sandbox
