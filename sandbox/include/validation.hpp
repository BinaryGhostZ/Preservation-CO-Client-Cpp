#pragma once

#include "assets.hpp"
#include "character.hpp"
#include "map.hpp"
#include <utility>
#include <cstdint>

namespace sandbox {
namespace validation {

void movement(const Map& map, const Character& character, std::pair<int32_t, int32_t> spawn);

} // namespace validation
} // namespace sandbox
