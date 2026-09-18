#include "ini.hpp"
#include <sstream>

namespace sandbox_data {
namespace ini {

std::optional<std::string> IniSection::first(const std::string& key) const {
    std::string key_lower = key;
    std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(), [](unsigned char c) { return std::tolower(c); });

    for (const auto& prop : properties) {
        std::string prop_key_lower = prop.key;
        std::transform(prop_key_lower.begin(), prop_key_lower.end(), prop_key_lower.begin(), [](unsigned char c) { return std::tolower(c); });
        
        if (prop_key_lower == key_lower) {
            return prop.value;
        }
    }
    return std::nullopt;
}

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

SectionCatalog SectionCatalog::parse(const std::vector<uint8_t>& bytes) {
    return parse(std::string(bytes.begin(), bytes.end()));
}

SectionCatalog SectionCatalog::parse(const std::string& text) {
    SectionCatalog catalog;
    std::istringstream stream(text);
    std::string line;
    
    IniSection* current_section = nullptr;

    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#' || trimmed.starts_with("//")) {
            continue;
        }

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            std::string name = trim(trimmed.substr(1, trimmed.size() - 2));
            if (!name.empty()) {
                catalog.sections_.push_back({name, {}});
                current_section = &catalog.sections_.back();
            }
            continue;
        }

        size_t eq_pos = trimmed.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = trim(trimmed.substr(0, eq_pos));
            std::string value = trim(trimmed.substr(eq_pos + 1));
            
            if (!key.empty() && current_section) {
                current_section->properties.push_back({key, value});
            }
        }
    }

    return catalog;
}

std::optional<IniSection> SectionCatalog::section(const std::string& name) const {
    std::string name_lower = name;
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), [](unsigned char c) { return std::tolower(c); });

    for (const auto& sec : sections_) {
        std::string sec_lower = sec.name;
        std::transform(sec_lower.begin(), sec_lower.end(), sec_lower.begin(), [](unsigned char c) { return std::tolower(c); });
        
        if (sec_lower == name_lower) {
            return sec;
        }
    }
    return std::nullopt;
}

IdPathCatalog IdPathCatalog::parse(const std::vector<uint8_t>& bytes) {
    return parse(std::string(bytes.begin(), bytes.end()));
}

IdPathCatalog IdPathCatalog::parse(const std::string& text) {
    IdPathCatalog catalog;
    std::istringstream stream(text);
    std::string line;

    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#' || (trimmed.front() == '[' && trimmed.back() == ']')) {
            continue;
        }

        size_t eq_pos = trimmed.find('=');
        if (eq_pos != std::string::npos) {
            std::string id_str = trim(trimmed.substr(0, eq_pos));
            std::string path = trim(trimmed.substr(eq_pos + 1));
            
            try {
                uint32_t id = std::stoul(id_str);
                // In C++, VirtualPath is just std::string with forward slashes
                std::replace(path.begin(), path.end(), '\\', '/');
                catalog.entries_[id] = path;
            } catch (...) {
                // Ignore parse errors
            }
        }
    }

    return catalog;
}

std::optional<std::string> IdPathCatalog::get(uint32_t id) const {
    auto it = entries_.find(id);
    if (it != entries_.end()) {
        return it->second;
    }
    return std::nullopt;
}

} // namespace ini
} // namespace sandbox_data
