#pragma once

#include <optional>
#include <vector>

#include "input/input_types.h"
#include "runtime/runtime_types.h"

namespace game2048 {

class RuntimeEventMapper {
public:
    static constexpr double kRepeatDelaySeconds = 0.20;
    static constexpr double kRepeatPeriodSeconds = 0.075;

    std::vector<RuntimeEvent> BuildEvents(const InputFrame& frame, OverlayMode overlayMode, double nowSeconds);

private:
    std::optional<RuntimeEvent> EventForCommand(InputCommand command, OverlayMode overlayMode) const;

    std::optional<Direction> lastHeldDir_ {};
    double repeatDeadline_ = 0.0;
};

}  // namespace game2048
