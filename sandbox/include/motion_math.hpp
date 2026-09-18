#pragma once

#include <chrono>
#include <optional>
#include <cstdint>

namespace sandbox {

constexpr float WALK_CELLS_PER_SECOND = 3.0f;
constexpr float RUN_CELLS_PER_SECOND = 6.0f;
constexpr float MAX_JUMP_DISTANCE = 16.0f;
constexpr int32_t MAX_JUMP_CLIMB = 120;
constexpr int32_t MAX_JUMP_RISE = 210;

struct JumpMotion {
    std::chrono::duration<float> duration;
    float peak_screen_pixels;
};

std::chrono::duration<float> step_duration(bool running);
JumpMotion jump_motion(float start[2], float target[2], std::chrono::duration<float> animation_duration);
float jump_screen_height(float peak_screen_pixels, float progress);
float cell_distance(float start[2], float target[2]);
void clamp_jump_target(float start[2], float target[2], float result[2]);
uint8_t direction_towards(float start[2], float target[2]);
std::optional<float> jump_facing(float start[2], float target[2]);

} // namespace sandbox
