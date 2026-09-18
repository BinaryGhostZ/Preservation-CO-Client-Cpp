#pragma once

#include <cstdint>
#include <optional>

namespace sandbox_data {

uint32_t resolve_weapon_action(std::optional<uint32_t> right, std::optional<uint32_t> left);

} // namespace sandbox_data
