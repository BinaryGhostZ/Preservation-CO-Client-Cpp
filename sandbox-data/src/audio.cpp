#include "audio.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace sandbox_data {
namespace audio {

namespace {

std::string trim(const std::string& str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(static_cast<unsigned char>(*start))) {
        start++;
    }
    if (start == str.end()) return "";
    auto end = str.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(static_cast<unsigned char>(*end)));
    return std::string(start, end + 1);
}

bool is_absent(const std::string& value) {
    std::string trimmed = trim(value);
    std::string lower = trimmed;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return lower == "null" || lower == "none";
}

std::optional<uint32_t> parse_id(const std::string& value) {
    try {
        size_t pos = 0;
        unsigned long id = std::stoul(trim(value), &pos);
        if (pos > 0) return static_cast<uint32_t>(id);
    } catch (...) {}
    return std::nullopt;
}

} // namespace

std::optional<std::string> normalize_content_key(const std::string& value) {
    std::string normalized = trim(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return c == '\\' ? '/' : std::tolower(c); });

    if (normalized.empty() || normalized[0] == '/') return std::nullopt;
    
    size_t pos = 0;
    while ((pos = normalized.find('/', pos)) != std::string::npos) {
        if (pos == normalized.length() - 1) return std::nullopt; // trailing slash
        if (normalized.substr(pos, 3) == "/..") return std::nullopt; // contains ..
        if (normalized.substr(pos, 2) == "//") return std::nullopt; // contains empty segment
        pos++;
    }
    if (normalized.substr(0, 2) == "..") return std::nullopt;
    
    return normalized;
}

ActionSoundCatalog ActionSoundCatalog::parse(const std::vector<uint8_t>& sound_ini, const std::vector<uint8_t>& action_sound_ini) {
    ActionSoundCatalog catalog;
    catalog.parse_file("sound.ini", sound_ini);
    catalog.parse_file("ActionSound.ini", action_sound_ini);
    return catalog;
}

ActionSoundCatalog ActionSoundCatalog::parse_action_sound(const std::vector<uint8_t>& bytes) {
    ActionSoundCatalog catalog;
    catalog.parse_file("ActionSound.ini", bytes);
    return catalog;
}

void ActionSoundCatalog::parse_file(const char* source, const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) return;
    std::string text(bytes.begin(), bytes.end());
    std::istringstream stream(text);
    std::string raw_line;
    size_t line = 0;
    
    while (std::getline(stream, raw_line)) {
        line++;
        if (!raw_line.empty() && raw_line.back() == '\r') {
            raw_line.pop_back();
        }
        std::string line_text = trim(raw_line);
        if (line_text.empty() || line_text[0] == ';' || line_text[0] == '#') {
            continue;
        }
        
        size_t eq_pos = line_text.find('=');
        if (eq_pos == std::string::npos) {
            diagnostics.push_back({source, line, "expected body.weapon.action=path"});
            continue;
        }
        
        std::string raw_key = trim(line_text.substr(0, eq_pos));
        std::string raw_value = trim(line_text.substr(eq_pos + 1));
        
        std::vector<std::string> parts;
        size_t start = 0, end = 0;
        while ((end = raw_key.find('.', start)) != std::string::npos) {
            parts.push_back(trim(raw_key.substr(start, end - start)));
            start = end + 1;
        }
        parts.push_back(trim(raw_key.substr(start)));
        
        if (parts.size() != 3) {
            diagnostics.push_back({source, line, "expected three numeric key components: " + raw_key});
            continue;
        }
        
        auto body_id = parse_id(parts[0]);
        if (!body_id) {
            invalid_key(source, line, raw_key, "body id");
            continue;
        }
        
        auto weapon_id = parse_id(parts[1]);
        if (!weapon_id) {
            invalid_key(source, line, raw_key, "weapon id");
            continue;
        }
        
        auto action_id = parse_id(parts[2]);
        if (!action_id) {
            invalid_key(source, line, raw_key, "action id");
            continue;
        }
        
        if (is_absent(raw_value)) {
            continue;
        }
        
        auto content_key = normalize_content_key(raw_value);
        if (!content_key) {
            diagnostics.push_back({source, line, "invalid sound virtual path: " + raw_value});
            continue;
        }
        
        ActionSoundKey key{*body_id, *weapon_id, *action_id};
        if (entries.find(key) != entries.end()) {
            diagnostics.push_back({source, line, "duplicate action tuple " + std::to_string(*body_id) + "." + std::to_string(*weapon_id) + "." + std::to_string(*action_id)});
        } else {
            entries[key] = ActionSoundEntry{key, *content_key, line};
        }
    }
}

void ActionSoundCatalog::invalid_key(const char* source, size_t line, const std::string& key, const char* component) {
    diagnostics.push_back({source, line, std::string("invalid ") + component + " in " + key});
}

std::optional<std::string> ActionSoundCatalog::get(const ActionSoundKey& key) const {
    auto it = entries.find(key);
    if (it != entries.end()) {
        return it->second.content_key;
    }
    return std::nullopt;
}

} // namespace audio
} // namespace sandbox_data
