#pragma once

#include <cstdint>
#include <optional>

namespace sandbox {

enum class Action {
    Idle,
    WalkLeft,
    WalkRight,
    RunLeft,
    RunRight,
    Jump,
};

inline size_t action_index(Action action) {
    switch (action) {
        case Action::Idle: return 0;
        case Action::WalkLeft: return 1;
        case Action::WalkRight: return 2;
        case Action::RunLeft: return 3;
        case Action::RunRight: return 4;
        case Action::Jump: return 5;
    }
    return 0;
}

inline uint32_t action_catalog_id(Action action) {
    switch (action) {
        case Action::Idle: return 100;
        case Action::WalkLeft: return 110;
        case Action::WalkRight: return 111;
        case Action::RunLeft: return 120;
        case Action::RunRight: return 121;
        case Action::Jump: return 130;
    }
    return 100;
}

inline std::optional<Action> paired_action(Action action) {
    switch (action) {
        case Action::WalkRight: return Action::WalkLeft;
        case Action::RunRight: return Action::RunLeft;
        default: return std::nullopt;
    }
}

} // namespace sandbox
