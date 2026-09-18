#include "movement.hpp"
#include "motion_math.hpp"
#include "map.hpp"
#include <queue>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <numbers>

namespace sandbox {

// Hash support for int pair
struct PairHash {
    size_t operator()(const std::pair<int32_t, int32_t>& p) const {
        return std::hash<int32_t>()(p.first) ^ (std::hash<int32_t>()(p.second) << 1);
    }
};

static std::vector<std::pair<int32_t, int32_t>> line(std::pair<int32_t, int32_t> start, std::pair<int32_t, int32_t> end) {
    int32_t steps = std::max({std::abs(end.first - start.first), std::abs(end.second - start.second), 1});
    std::vector<std::pair<int32_t, int32_t>> result;
    result.reserve(steps + 1);
    for (int32_t i = 0; i <= steps; ++i) {
        float f = static_cast<float>(i) / steps;
        result.push_back({
            static_cast<int32_t>(std::round(start.first + (end.first - start.first) * f)),
            static_cast<int32_t>(std::round(start.second + (end.second - start.second) * f))
        });
    }
    return result;
}

static bool jump_allowed(const Map& map, std::pair<int32_t, int32_t> start, std::pair<int32_t, int32_t> target) {
    if (start == target || !map.walkable(std::array<int32_t, 2>{target.first, target.second})) return false;
    
    float fstart[2] = {static_cast<float>(start.first), static_cast<float>(start.second)};
    float ftarget[2] = {static_cast<float>(target.first), static_cast<float>(target.second)};
    if (cell_distance(fstart, ftarget) > MAX_JUMP_DISTANCE) return false;

    auto a = map.data.cell(start.first, start.second);
    auto b = map.data.cell(target.first, target.second);
    if (!a || !b) return false;

    if (b->elevation - a->elevation > MAX_JUMP_CLIMB) return false;

    int32_t ceiling = std::max(a->elevation, b->elevation) + MAX_JUMP_RISE;
    auto l = line(start, target);
    for (size_t i = 1; i < l.size(); ++i) {
        auto c = map.data.cell(l[i].first, l[i].second);
        if (c && c->elevation > ceiling) return false;
    }
    return true;
}

template<typename F>
static std::optional<std::vector<std::pair<int32_t, int32_t>>> find_route(
    std::pair<int32_t, int32_t> start, 
    std::pair<int32_t, int32_t> goal, 
    F open) 
{
    if (!open(goal)) return std::nullopt;

    auto heuristic = [&](std::pair<int32_t, int32_t> p) {
        return std::max(std::abs(p.first - goal.first), std::abs(p.second - goal.second)) * 10;
    };

    using Node = std::tuple<int32_t, int32_t, std::pair<int32_t, int32_t>>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> queue;
    
    queue.push({heuristic(start), 0, start});
    std::unordered_map<std::pair<int32_t, int32_t>, int32_t, PairHash> costs = {{start, 0}};
    std::unordered_map<std::pair<int32_t, int32_t>, std::pair<int32_t, int32_t>, PairHash> parents;

    for (int it = 0; it < 100000; ++it) {
        if (queue.empty()) break;
        
        auto [f, cost, p] = queue.top();
        queue.pop();
        
        if (cost != costs[p]) continue;
        
        if (p == goal) {
            std::vector<std::pair<int32_t, int32_t>> path;
            auto curr = p;
            while (curr != start) {
                path.push_back(curr);
                curr = parents[curr];
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) continue;
                std::pair<int32_t, int32_t> q = {p.first + dx, p.second + dy};
                
                if (!open(q) || (dx != 0 && dy != 0 && (!open({p.first + dx, p.second}) || !open({p.first, p.second + dy})))) {
                    continue;
                }
                
                int32_t next = cost + ((dx != 0 && dy != 0) ? 14 : 10);
                auto it_cost = costs.find(q);
                if (it_cost == costs.end() || next < it_cost->second) {
                    costs[q] = next;
                    parents[q] = p;
                    queue.push({next + heuristic(q), next, q});
                }
            }
        }
    }
    return std::nullopt;
}

Player::Player(const Map& map, std::optional<std::pair<int32_t, int32_t>> spawn) {
    std::pair<int32_t, int32_t> p;
    if (spawn) {
        p = *spawn;
    } else {
        int32_t center[2] = {static_cast<int32_t>(map.data.width / 2), static_cast<int32_t>(map.data.height / 2)};
        bool found = false;
        int32_t max_r = std::max(map.data.width, map.data.height);
        for (int32_t r = 0; r < max_r && !found; ++r) {
            for (int32_t i = -r; i <= r && !found; ++i) {
                std::pair<int32_t, int32_t> pts[] = {
                    {center[0] + i, center[1] - r},
                    {center[0] + i, center[1] + r},
                    {center[0] - r, center[1] + i},
                    {center[0] + r, center[1] + i}
                };
                for (auto pt : pts) {
                    if (map.walkable(std::array<int32_t, 2>{pt.first, pt.second})) {
                        p = pt;
                        found = true;
                        break;
                    }
                }
            }
        }
        if (!found) throw std::runtime_error("Map has no walkable spawn");
    }

    if (!map.walkable(std::array<int32_t, 2>{p.first, p.second})) throw std::runtime_error("Requested spawn is blocked");

    position[0] = static_cast<float>(p.first);
    position[1] = static_cast<float>(p.second);
    facing = -std::numbers::pi_v<float> / 4.0f;
    running = true;
    height = 0.0f;
    action = Action::Idle;
    action_instance = 0;
    right_foot = false;
}

std::pair<int32_t, int32_t> Player::cell() const {
    return {static_cast<int32_t>(std::round(position[0])), static_cast<int32_t>(std::round(position[1]))};
}

bool Player::moving() const {
    return segment.has_value() || !route.empty();
}

std::optional<float> Player::progress() const {
    if (segment) return std::clamp(segment->elapsed / segment->duration, 0.0f, 1.0f);
    return std::nullopt;
}

void Player::stop() {
    route.clear();
}

void Player::go(const Map& map, std::pair<int32_t, int32_t> target) {
    auto start = segment ? std::pair<int32_t, int32_t>{static_cast<int32_t>(segment->target[0]), static_cast<int32_t>(segment->target[1])} : cell();
    auto p = find_route(start, target, [&](std::pair<int32_t, int32_t> p) { return map.walkable(std::array<int32_t, 2>{p.first, p.second}); });
    if (p) {
        route.assign(p->begin(), p->end());
    }
}

void Player::jump(const Map& map, std::pair<int32_t, int32_t> requested, std::chrono::duration<float> animation) {
    if (segment) return;

    auto start = cell();
    float req_f[2] = {static_cast<float>(requested.first), static_cast<float>(requested.second)};
    float clamped_f[2];
    clamp_jump_target(position, req_f, clamped_f);
    
    std::pair<int32_t, int32_t> clamped = {static_cast<int32_t>(std::round(clamped_f[0])), static_cast<int32_t>(std::round(clamped_f[1]))};
    
    auto l = line(start, clamped);
    std::optional<std::pair<int32_t, int32_t>> target;
    for (auto it = l.rbegin(); it != l.rend(); ++it) {
        if (*it == start) continue;
        if (jump_allowed(map, start, *it)) {
            target = *it;
            break;
        }
    }
    
    if (!target) return;

    float tf[2] = {static_cast<float>(target->first), static_cast<float>(target->second)};
    auto motion = jump_motion(position, tf, animation);
    
    auto f = jump_facing(position, tf);
    if (f) facing = *f;
    
    route.clear();
    action = Action::Jump;
    action_instance++;
    segment = Segment{
        {position[0], position[1]},
        {tf[0], tf[1]},
        0.0f,
        motion.duration.count(),
        motion.peak_screen_pixels,
        true
    };
}

void Player::face(std::pair<int32_t, int32_t> target) {
    if (!moving()) {
        float tf[2] = {static_cast<float>(target.first), static_cast<float>(target.second)};
        facing = -(direction_towards(position, tf) + 1) * (std::numbers::pi_v<float> / 4.0f);
    }
}

void Player::tick(float dt) {
    float remaining = std::clamp(dt, 0.0f, 0.1f);
    while (true) {
        if (!segment) {
            if (!route.empty()) {
                auto target = route.front();
                route.pop_front();
                float tf[2] = {static_cast<float>(target.first), static_cast<float>(target.second)};
                facing = -(direction_towards(position, tf) + 1) * (std::numbers::pi_v<float> / 4.0f);
                
                if (!running && !right_foot) action = Action::WalkLeft;
                else if (!running && right_foot) action = Action::WalkRight;
                else if (running && !right_foot) action = Action::RunLeft;
                else action = Action::RunRight;
                
                right_foot = !right_foot;
                action_instance++;
                segment = Segment{
                    {position[0], position[1]},
                    {tf[0], tf[1]},
                    0.0f,
                    step_duration(running).count(),
                    0.0f,
                    false
                };
            } else {
                action = Action::Idle;
                return;
            }
        }
        
        float used = std::min(remaining, segment->duration - segment->elapsed);
        segment->elapsed += used;
        remaining -= used;
        
        float progress = std::min(1.0f, segment->elapsed / segment->duration);
        position[0] = segment->start[0] + (segment->target[0] - segment->start[0]) * progress;
        position[1] = segment->start[1] + (segment->target[1] - segment->start[1]) * progress;
        
        if (segment->jump) {
            height = jump_screen_height(segment->peak, progress);
        } else {
            height = 0.0f;
        }
        
        if (progress >= 1.0f) {
            position[0] = segment->target[0];
            position[1] = segment->target[1];
            height = 0.0f;
            segment = std::nullopt;
            action = Action::Idle;
        }
        
        if (segment) break;
    }
}

} // namespace sandbox
