#pragma once

namespace game2048 {

struct GamepadBindingMap {
    int faceBottom = 0;
    int faceRight = 1;
    int faceLeft = 2;
    int faceTop = 3;
    int leftShoulder = 4;
    int rightShoulder = 5;
    int back = 6;
    int start = 7;
    int dpadUp = 10;
    int dpadRight = 11;
    int dpadDown = 12;
    int dpadLeft = 13;
};

GamepadBindingMap DefaultGamepadBindings();

}  // namespace game2048
