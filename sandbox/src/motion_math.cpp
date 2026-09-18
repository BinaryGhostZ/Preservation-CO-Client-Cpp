#include "motion_math.hpp"
#include <cmath>
#include <algorithm>
#include <numbers>

namespace sandbox {

constexpr float WORLD_PIXELS_PER_UNIT = 32.0f;
constexpr float JUMP_ARC_SCALE = 0.6f;
constexpr float FRAC_PI_4 = std::numbers::pi_v<float> / 4.0f;

static float world_distance(float start[2], float target[2]) {
    float dx = target[0] - start[0];
    float dy = target[1] - start[1];
    return std::hypot(dx - dy, (dx + dy) * 0.5f);
}

std::chrono::duration<float> step_duration(bool running) {
    float cells_per_second = running ? RUN_CELLS_PER_SECOND : WALK_CELLS_PER_SECOND;
    return std::chrono::duration<float>(1.0f / cells_per_second);
}

JumpMotion jump_motion(float start[2], float target[2], std::chrono::duration<float> animation_duration) {
    float distance = world_distance(start, target);
    float zero[2] = {0.0f, 0.0f};
    float max_dist_target[2] = {MAX_JUMP_DISTANCE, 0.0f};
    float maximum_world_distance = world_distance(zero, max_dist_target);
    
    float distance_fraction = std::max(0.4f, distance / maximum_world_distance);
    float duration = std::max(0.5f, animation_duration.count() * 2.0f * distance_fraction);
    float peak_world_units = std::max(0.5f, distance * JUMP_ARC_SCALE);

    return {
        std::chrono::duration<float>(duration),
        peak_world_units * std::sin(FRAC_PI_4) * WORLD_PIXELS_PER_UNIT
    };
}

float jump_screen_height(float peak_screen_pixels, float progress) {
    return peak_screen_pixels * std::sin(std::numbers::pi_v<float> * std::clamp(progress, 0.0f, 1.0f));
}

float cell_distance(float start[2], float target[2]) {
    return std::hypot(target[0] - start[0], target[1] - start[1]);
}

void clamp_jump_target(float start[2], float target[2], float result[2]) {
    float distance = cell_distance(start, target);
    if (distance <= MAX_JUMP_DISTANCE || distance <= 1e-6f) {
        result[0] = target[0];
        result[1] = target[1];
        return;
    }
    float scale = MAX_JUMP_DISTANCE / distance;
    result[0] = (target[0] - start[0]) * scale + start[0];
    result[1] = (target[1] - start[1]) * scale + start[1];
}

uint8_t direction_towards(float start[2], float target[2]) {
    float delta_x = target[0] - start[0];
    float delta_y = target[1] - start[1];

    if (std::abs(delta_x) <= 1e-6f && std::abs(delta_y) <= 1e-6f) return 3;

    float angle = std::atan2(delta_y, delta_x) - std::numbers::pi_v<float> / 2.0f;
    if (angle < 0.0f) angle += 2.0f * std::numbers::pi_v<float>;
    return static_cast<uint8_t>(std::round(angle / (std::numbers::pi_v<float> / 4.0f))) % 8;
}

std::optional<float> jump_facing(float start[2], float target[2]) {
    float delta_x = target[0] - start[0];
    float delta_y = target[1] - start[1];
    
    if (std::abs(delta_x) <= 1e-6f && std::abs(delta_y) <= 1e-6f) return std::nullopt;

    float world_x = delta_x - delta_y;
    float world_depth = (delta_x + delta_y) * 0.5f;
    return std::atan2(world_x, world_depth);
}

} // namespace sandbox
