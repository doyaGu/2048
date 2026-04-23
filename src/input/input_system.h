#pragma once

#include "input/gamepad_input.h"
#include "input/pointer_input.h"
#include "app/session_types.h"

namespace game2048 {

class InputSystem {
public:
    InputSystem();

    InputFrame BuildFrame(const RawInputState& raw,
                          const LayoutMetrics& layout,
                          bool touchHudActive,
                          bool animationBlocksInput,
                          OverlayMode overlayMode);

private:
    InputCommand ResolveKeyboardCommand(const RawInputState& raw) const;
    std::optional<Direction> ResolveKeyboardPressedMove(const RawInputState& raw) const;
    std::optional<Direction> ResolveKeyboardHeldMove(const RawInputState& raw) const;
    InputCommand ResolveGamepadCommand(const RawInputState& raw, OverlayMode overlayMode, ControlId& primaryControl) const;

    PointerInputRouter pointer_ {};
    GamepadInputRouter gamepad_;
    PointerGestureState gesture_ {};
};

}  // namespace game2048
