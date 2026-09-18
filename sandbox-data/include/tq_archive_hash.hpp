#pragma once

#include <string_view>
#include <cstdint>

namespace sandbox_data {

uint32_t path_id(std::string_view normalized_path, const char* context);

} // namespace sandbox_data
