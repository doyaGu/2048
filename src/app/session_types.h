#pragma once

namespace game2048 {

enum class ControlMode {
    Human,
    AIAutoplay,
    AISingleStep,
};

enum class OverlayMode {
    None,
    Help,
    Victory,
    GameOver,
};

enum class InputGate {
    Accepting,
    BlockedByOverlay,
    BlockedByAnimation,
};

}  // namespace game2048