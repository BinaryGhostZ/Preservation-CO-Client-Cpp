#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace sandbox_data {
namespace audio {

struct ActionSoundKey {
    uint32_t body_id;
    uint32_t weapon_id;
    uint32_t action_id;

    bool operator==(const ActionSoundKey& other) const {
        return body_id == other.body_id && weapon_id == other.weapon_id && action_id == other.action_id;
    }
};

struct ActionSoundKeyHasher {
    std::size_t operator()(const ActionSoundKey& k) const {
        return (static_cast<std::size_t>(k.body_id) << 32) ^ 
               (static_cast<std::size_t>(k.weapon_id) << 16) ^ 
               (static_cast<std::size_t>(k.action_id));
    }
};

struct ActionSoundEntry {
    ActionSoundKey key;
    std::string content_key;
    size_t line;
};

struct AudioCatalogDiagnostic {
    std::string source;
    size_t line;
    std::string detail;
};

class ActionSoundCatalog {
public:
    static ActionSoundCatalog parse(const std::vector<uint8_t>& sound_ini, const std::vector<uint8_t>& action_sound_ini);
    static ActionSoundCatalog parse_action_sound(const std::vector<uint8_t>& bytes);

    std::optional<std::string> get(const ActionSoundKey& key) const;
    const std::vector<AudioCatalogDiagnostic>& get_diagnostics() const { return diagnostics; }

private:
    void parse_file(const char* source, const std::vector<uint8_t>& bytes);
    void invalid_key(const char* source, size_t line, const std::string& key, const char* component);

    std::unordered_map<ActionSoundKey, ActionSoundEntry, ActionSoundKeyHasher> entries;
    std::vector<AudioCatalogDiagnostic> diagnostics;
};

std::optional<std::string> normalize_content_key(const std::string& value);

} // namespace audio
} // namespace sandbox_data
