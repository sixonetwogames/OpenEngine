#pragma once
#include "types.h"

class InputHandler {
public:
    void Init();
    InputState Poll();

    void SetMouseSensitivity(Vector2 sens);
    void SetStickSensitivity(Vector2 sens);
    void SetStickDeadzone(float deadzone);

private:
    Vector2 mouseSens = {0.001f, 0.001f};
    Vector2 stickSens = {2.5f, 2.0f};
    float   deadzone  = 0.15f;
    int     gamepad   = 0;

    float ApplyDeadzone(float value) const;
};
