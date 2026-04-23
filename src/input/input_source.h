#pragma once

#include "input/input_types.h"

namespace game2048 {

class InputSource {
public:
    RawInputState Poll();

private:
    bool previousTouchDown_ = false;
    InputPoint previousTouchPosition_ {};
};

}  // namespace game2048
