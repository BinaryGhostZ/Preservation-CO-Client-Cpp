#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <cstdint>
#include <algorithm>

namespace sandbox_data {
namespace ini {

struct IniProperty {
    std::string key;
    std::string value;
};

struct IniSection {
    std::string name;
    std::vector<IniProperty> properties;

    std::optional<std::string> first(const std::string& key) const;
};

class SectionCatalog {
    std::vector<IniSection> sections_;

public:
    static SectionCatalog parse(const std::vector<uint8_t>& bytes);
    static SectionCatalog parse(const std::string& text);

    const std::vector<IniSection>& sections() const { return sections_; }
    std::optional<IniSection> section(const std::string& name) const;
};

class IdPathCatalog {
    std::unordered_map<uint32_t, std::string> entries_;

public:
    static IdPathCatalog parse(const std::vector<uint8_t>& bytes);
    static IdPathCatalog parse(const std::string& text);

    std::optional<std::string> get(uint32_t id) const;
};

} // namespace ini
} // namespace sandbox_data
