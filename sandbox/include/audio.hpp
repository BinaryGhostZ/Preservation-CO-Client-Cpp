#pragma once

#include "assets.hpp"
#include "character.hpp"
#include "movement.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

namespace sandbox {

class MovementAudio {
public:
    MovementAudio();
    ~MovementAudio();

    // Prevent copying because we manage miniaudio resources
    MovementAudio(const MovementAudio&) = delete;
    MovementAudio& operator=(const MovementAudio&) = delete;

    MovementAudio(MovementAudio&& other) noexcept;
    MovementAudio& operator=(MovementAudio&& other) noexcept;

    void load(Assets& assets, const Character& character);
    void update(const Player& player);
    void check() const;

private:
    struct AudioContext;
    AudioContext* ctx; // Pimpl to hide miniaudio.h from header
};

} // namespace sandbox
