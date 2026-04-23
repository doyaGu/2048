#pragma once

#include <optional>

#include "input_bindings.h"
#include "input_types.h"

namespace game2048 {

class GamepadInputRouter {
public:
    explicit GamepadInputRouter(GamepadBindingMap bindings);

    std::optional<Direction> ResolvePressedMove(const RawGamepadState& state);
    std::optional<Direction> ResolveHeldMove(const RawGamepadState& state) const;
    InputCommand ResolveCommand(const RawGamepadState& state) const;

private:
    GamepadBindingMap bindings_ {};
    bool stickReady_ = true;
};

}  // namespace game2048
