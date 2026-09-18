#include "weapon_action.hpp"

namespace sandbox_data {

uint32_t resolve_weapon_action(std::optional<uint32_t> right, std::optional<uint32_t> left) {
    if (right && *right == 0) right = std::nullopt;
    if (left && *left == 0) left = std::nullopt;

    if (right && !left) return *right / 1000;
    if (!right && left) return 741;
    if (right && left) {
        uint32_t r = *right;
        uint32_t l = *left;
        if (l / 100000 == 9) return 700 + r / 10000;
        if (l / 100000 == 4) return 600 + (r % 100000) / 10000 * 10 + (l % 100000) / 10000;
        if (r / 1000 == 500) return 500;
    }
    return 0;
}

} // namespace sandbox_data
