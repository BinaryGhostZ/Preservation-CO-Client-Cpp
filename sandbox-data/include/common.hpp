#pragma once

#include <stdexcept>
#include <string>

namespace sandbox_data {

inline constexpr uint32_t MAXIMUM_MAP_OBJECTS = 65536;

class ContentError : public std::runtime_error {
public:
    ContentError(const std::string& context, const std::string& detail)
        : std::runtime_error(context + ": " + detail) {}
};

class VirtualPath {
    std::string path_;
public:
    VirtualPath() = default;
    VirtualPath(std::string path) : path_(std::move(path)) {}
    const std::string& as_string() const { return path_; }
    const char* as_str() const { return path_.c_str(); }
};

} // namespace sandbox_data
