#include "validation.hpp"
#include "movement.hpp"
#include "action.hpp"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <vector>
#include <algorithm>
#include <iomanip>

namespace sandbox {
namespace validation {

void movement(const Map& map, const Character& character, std::pair<int32_t, int32_t> spawn_pair) {
    std::array<int32_t, 2> spawn = {spawn_pair.first, spawn_pair.second};
    auto world = map.project({static_cast<float>(spawn[0]), static_cast<float>(spawn[1])});
    auto roundtrip = map.unproject(world);
    
    int32_t rx = static_cast<int32_t>(std::round(roundtrip[0]));
    int32_t ry = static_cast<int32_t>(std::round(roundtrip[1]));
    
    if (rx != spawn[0] || ry != spawn[1]) {
        throw std::runtime_error("Projection roundtrip failed");
    }
    
    std::optional<std::array<int32_t, 2>> target_opt;
    for (int32_t x = -6; x <= 6; ++x) {
        for (int32_t y = -6; y <= 6; ++y) {
            std::array<int32_t, 2> p = {spawn[0] + x, spawn[1] + y};
            if (p != spawn && map.walkable(p)) {
                target_opt = p;
                break;
            }
        }
        if (target_opt) break;
    }
    
    if (!target_opt) {
        throw std::runtime_error("No nearby movement test cell");
    }
    auto target = *target_opt;
    
    for (bool running : {false, true}) {
        Player player(map, spawn_pair);
        player.running = running;
        player.go(map, {target[0], target[1]});
        
        if (!player.moving()) {
            throw std::runtime_error("Asset-backed route was not created");
        }
        
        uint64_t last_instance = 0;
        std::vector<Action> steps;
        
        for (int i = 0; i < 3600; ++i) {
            player.tick(1.0f / 120.0f);
            if (player.action_instance != last_instance) {
                Action expected;
                if (!running && steps.size() % 2 == 0) expected = Action::WalkLeft;
                else if (!running && steps.size() % 2 == 1) expected = Action::WalkRight;
                else if (running && steps.size() % 2 == 0) expected = Action::RunLeft;
                else expected = Action::RunRight;
                
                if (player.action != expected) {
                    throw std::runtime_error("Movement did not alternate left/right actions");
                }
                last_instance = player.action_instance;
                steps.push_back(player.action);
            }
            if (!player.moving()) {
                break;
            }
        }
        
        if (player.cell() != std::pair<int32_t, int32_t>{target[0], target[1]}) {
            throw std::runtime_error("Route did not reach destination");
        }
        if (steps.size() < 2) {
            throw std::runtime_error("Movement check needs both foot actions");
        }
    }
    
    Action actions[] = {
        Action::Idle, Action::WalkLeft, Action::WalkRight, Action::RunLeft, Action::RunRight, Action::Jump
    };
    
    for (Action action : actions) {
        for (const auto& part : character.parts) {
            for (float progress : {0.0f, 0.5f, 1.0f}) {
                auto vertices = character.vertices(part, 0.0f, action, 0.0f, progress);
                if (vertices.empty()) {
                    throw std::runtime_error("Movement pose contains invalid geometry");
                }
                for (const auto& [p, uv] : vertices) {
                    for (int i = 0; i < 3; ++i) {
                        if (!std::isfinite(p[i])) throw std::runtime_error("Movement pose contains invalid geometry");
                    }
                    for (int i = 0; i < 2; ++i) {
                        if (!std::isfinite(uv[i])) throw std::runtime_error("Movement pose contains invalid geometry");
                    }
                }
            }
        }
    }
    
    Player player(map, spawn_pair);
    player.jump(map, {target[0], target[1]}, std::chrono::duration<float>(character.duration(Action::Jump)));
    if (!player.moving()) {
        throw std::runtime_error("Asset-backed jump rejected");
    }
    
    float peak = 0.0f;
    for (int i = 0; i < 1200; ++i) {
        player.tick(1.0f / 120.0f);
        peak = std::max(peak, player.height);
        if (!player.moving()) break;
    }
    
    if (peak <= 0.0f || player.height != 0.0f || !map.walkable({player.cell().first, player.cell().second}) || player.cell() == spawn_pair) {
        throw std::runtime_error("Jump/landing validation failed");
    }
    
    std::cout << "Movement OK: walk, run, jump arc/landing, projection; jump peak " 
              << std::fixed << std::setprecision(1) << peak << "px" << std::endl;
}

} // namespace validation
} // namespace sandbox
