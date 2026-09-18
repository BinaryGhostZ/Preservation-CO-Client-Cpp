#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "audio.hpp"
#include "../../sandbox-data/include/audio.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <memory>

#include "action.hpp"

namespace sandbox {

std::optional<std::string> action_sound(
    const sandbox_data::audio::ActionSoundCatalog& catalog,
    uint32_t body_id,
    uint32_t weapon_id,
    Action action) {
    
    sandbox_data::audio::ActionSoundKey key{body_id, weapon_id, action_catalog_id(action)};
    auto path = catalog.get(key);
    if (!path) {
        sandbox_data::audio::ActionSoundKey fallback{body_id, 999, action_catalog_id(action)};
        path = catalog.get(fallback);
    }
    return path;
}

struct SoundVoice {
    std::vector<uint8_t> bytes;
    ma_decoder decoder;
    ma_sound sound;
    bool valid = false;
    
    ~SoundVoice() {
        if (valid) {
            ma_sound_uninit(&sound);
            ma_decoder_uninit(&decoder);
        }
    }
};

struct MovementAudio::AudioContext {
    ma_engine engine;
    bool engine_initialized = false;
    std::optional<std::string> paths[6];
    std::unordered_map<std::string, std::unique_ptr<SoundVoice>> voices;
    std::optional<std::pair<Action, uint64_t>> last_action;
    
    ~AudioContext() {
        voices.clear();
        if (engine_initialized) {
            ma_engine_uninit(&engine);
        }
    }
};

MovementAudio::MovementAudio() : ctx(new AudioContext()) {}

MovementAudio::~MovementAudio() {
    delete ctx;
}

MovementAudio::MovementAudio(MovementAudio&& other) noexcept : ctx(other.ctx) {
    other.ctx = nullptr;
}

MovementAudio& MovementAudio::operator=(MovementAudio&& other) noexcept {
    if (this != &other) {
        delete ctx;
        ctx = other.ctx;
        other.ctx = nullptr;
    }
    return *this;
}

void MovementAudio::load(Assets& assets, const Character& character) {
    std::vector<uint8_t> sound_ini;
    try {
        sound_ini = assets.read("ini/sound.ini");
    } catch (...) {}
    
    auto action_sound_ini = assets.read("ini/ActionSound.ini");
    
    auto catalog = sandbox_data::audio::ActionSoundCatalog::parse(sound_ini, action_sound_ini);
    
    for (int i = 0; i < 6; ++i) {
        Action a = static_cast<Action>(i);
        ctx->paths[i] = action_sound(catalog, character.body_id, character.weapon_action, a);
    }
    
    ma_result result = ma_engine_init(NULL, &ctx->engine);
    if (result == MA_SUCCESS) {
        ctx->engine_initialized = true;
    } else {
        std::cerr << "Audio unavailable: failed to initialize miniaudio engine." << std::endl;
    }
    
    for (int i = 0; i < 6; ++i) {
        if (!ctx->paths[i]) continue;
        const std::string& path = *ctx->paths[i];
        
        if (ctx->voices.find(path) != ctx->voices.end()) continue;
        
        try {
            auto bytes = assets.read(path);
            if (bytes.size() > 32 * 1024 * 1024) {
                throw std::runtime_error("Audio exceeds 32 MiB safety limit");
            }
            
            auto voice = std::make_unique<SoundVoice>();
            voice->bytes = std::move(bytes);
            
            if (ma_decoder_init_memory(voice->bytes.data(), voice->bytes.size(), NULL, &voice->decoder) != MA_SUCCESS) {
                throw std::runtime_error("Failed to decode audio bytes");
            }
            
            if (ctx->engine_initialized) {
                if (ma_sound_init_from_data_source(&ctx->engine, &voice->decoder, 0, NULL, &voice->sound) != MA_SUCCESS) {
                    ma_decoder_uninit(&voice->decoder);
                    throw std::runtime_error("Failed to init sound");
                }
                ma_sound_set_volume(&voice->sound, 0.8f);
                voice->valid = true;
            }
            
            ctx->voices[path] = std::move(voice);
            
        } catch (const std::exception& e) {
            std::cerr << "Audio: " << path << ": " << e.what() << std::endl;
        }
    }
}

void MovementAudio::update(const Player& player) {
    if (!ctx->engine_initialized) return;
    
    std::pair<Action, uint64_t> action = {player.action, player.action_instance};
    if (ctx->last_action && *ctx->last_action == action) {
        return;
    }
    ctx->last_action = action;
    
    auto& path_opt = ctx->paths[action_index(player.action)];
    if (!path_opt) return;
    
    auto it = ctx->voices.find(*path_opt);
    if (it == ctx->voices.end() || !it->second->valid) return;
    
    ma_sound_stop(&it->second->sound);
    ma_sound_seek_to_pcm_frame(&it->second->sound, 0);
    ma_sound_start(&it->second->sound);
}

void MovementAudio::check() const {
    for (int i = 0; i < 6; ++i) {
        if (ctx->paths[i]) {
            if (ctx->voices.find(*ctx->paths[i]) == ctx->voices.end()) {
                throw std::runtime_error("Could not decode movement sound " + *ctx->paths[i]);
            }
        }
    }
    std::cout << "Audio OK: " << ctx->voices.size() << " decoded movement sounds" << std::endl;
}

} // namespace sandbox
