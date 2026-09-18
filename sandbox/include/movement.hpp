#pragma once

#include "action.hpp"
#include "dmap.hpp"
#include <vector>
#include <deque>
#include <optional>
#include <chrono>

namespace sandbox {

struct Segment {
    float start[2];
    float target[2];
    float elapsed;
    float duration;
    float peak;
    bool jump;
};

class Map;

class Player {
public:
    float position[2];
    float facing;
    bool running;
    float height;
    Action action;
    uint64_t action_instance;
    bool right_foot;
    std::deque<std::pair<int32_t, int32_t>> route;
    std::optional<Segment> segment;

    Player(const Map& map, std::optional<std::pair<int32_t, int32_t>> spawn);

    std::pair<int32_t, int32_t> cell() const;
    bool moving() const;
    std::optional<float> progress() const;
    
    void stop();
    void go(const Map& map, std::pair<int32_t, int32_t> target);
    void jump(const Map& map, std::pair<int32_t, int32_t> requested, std::chrono::duration<float> animation);
    void face(std::pair<int32_t, int32_t> target);
    void tick(float dt);
};

} // namespace sandbox
