#pragma once

#include "wdf.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>

namespace sandbox {

struct Image {
    int width;
    int height;
    int channels;
    std::vector<uint8_t> data;
};

class Assets {
    std::filesystem::path root_;
    std::vector<std::pair<std::ifstream, sandbox_data::WdfDirectory>> archives_;
    std::unordered_map<std::string, std::filesystem::path> resolved_;

    std::optional<std::filesystem::path> loose(const std::string& name);

public:
    Assets(const std::filesystem::path& root);

    std::vector<uint8_t> read(const std::string& name);
    Image image(const std::string& name);
};

} // namespace sandbox
