#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "input/input_types.h"
#include "app/view_state.h"

namespace game2048 {

namespace detail {

template <typename Enum, std::size_t N>
constexpr std::string_view EnumName(const std::array<std::string_view, N>& values,
                                    Enum value,
                                    std::string_view fallback) {
    const auto index = static_cast<std::size_t>(value);
    return index < values.size() ? values[index] : fallback;
}

}  // namespace detail

inline constexpr std::array<std::string_view, 3> kAgentNames {{"Human", "Greedy", "Expectimax"}};
inline constexpr std::array<std::string_view, 3> kControlModeNames {{"Human", "Auto AI", "AI Step"}};
inline constexpr std::array<std::string_view, 4> kOverlayModeNames {{"None", "Help", "Victory", "Game Over"}};
inline constexpr std::array<std::string_view, 3> kInputGateNames {{"Ready", "Overlay", "Animating"}};
inline constexpr std::array<std::string_view, 4> kDirectionNames {{"Up", "Down", "Left", "Right"}};
inline constexpr std::array<std::string_view, 3> kAnimationSpeedNames {{"Normal", "Slow", "Turbo"}};
inline constexpr std::array<std::string_view, 15> kControlLabels {{
    "",
    "Up",
    "Down",
    "Left",
    "Right",
    "Restart",
    "Undo",
    "Auto",
    "Step",
    "AI",
    "Speed",
    "Help",
    "Close",
    "OK",
    "Back",
}};

inline constexpr std::string_view AgentName(HUDAgentKind kind) {
    return detail::EnumName(kAgentNames, kind, "Unknown");
}

inline constexpr std::string_view ControlModeName(HUDControlMode mode) {
    return detail::EnumName(kControlModeNames, mode, "Unknown");
}

inline constexpr std::string_view OverlayModeName(HUDOverlayMode mode) {
    return detail::EnumName(kOverlayModeNames, mode, "Unknown");
}

inline constexpr std::string_view InputGateName(HUDInputGate gate) {
    return detail::EnumName(kInputGateNames, gate, "Unknown");
}

inline constexpr std::string_view DirectionName(Direction direction) {
    return detail::EnumName(kDirectionNames, direction, "-");
}

inline constexpr std::string_view AnimationSpeedName(AnimationSpeed speed) {
    return detail::EnumName(kAnimationSpeedNames, speed, "Unknown");
}

inline constexpr std::string_view ControlLabel(ControlId id) {
    return detail::EnumName(kControlLabels, id, "");
}

}  // namespace game2048
