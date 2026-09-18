#pragma once

#include "action.hpp"
#include "assets.hpp"
#include "c3.hpp"
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <chrono>

namespace sandbox {

struct Part {
    sandbox_data::C3Mesh mesh;
    std::string texture;
    std::optional<sandbox_data::C3Motion> local;
    size_t track;
};

class Character {
public:
    std::vector<Part> parts;
    std::vector<sandbox_data::C3Document> actions;
    std::array<float, 6> intervals;
    uint32_t body_id;
    uint32_t weapon_action;
    std::string name;
    std::string guild;
    std::string guild_rank;
    uint32_t health;
    uint32_t maximum_health;

    static Character load(Assets& assets, uint32_t body);
    std::chrono::duration<float> duration(Action action) const;
    std::vector<std::pair<std::array<float, 3>, std::array<float, 2>>> vertices(const Part& part, float seconds, Action action, float facing, std::optional<float> progress) const;
};

} // namespace sandbox
