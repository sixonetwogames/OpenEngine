#include "input_handler.h"
#include <cmath>

void InputHandler::Init() {
    DisableCursor();
}

float InputHandler::ApplyDeadzone(float value) const {
    return (std::fabs(value) > deadzone) ? value : 0.0f;
}

InputState InputHandler::Poll() {
    InputState state = {0};

    // --- Keyboard input ---
    state.moveX  = (float)(IsKeyDown(KEY_D) - IsKeyDown(KEY_A));
    state.moveY  = (float)(IsKeyDown(KEY_W) - IsKeyDown(KEY_S));
    state.jump   = IsKeyPressed(KEY_SPACE);
    state.crouch = IsKeyDown(KEY_LEFT_CONTROL);

    Vector2 mouseDelta = GetMouseDelta();
    state.lookX = -mouseDelta.x * mouseSens.x;
    state.lookY =  mouseDelta.y * mouseSens.y;

    // --- Gamepad overlay (additive) ---
    if (IsGamepadAvailable(gamepad)) {
        float lx = ApplyDeadzone(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_X));
        float ly = ApplyDeadzone(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_Y));
        float rx = ApplyDeadzone(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_X));
        float ry = ApplyDeadzone(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_Y));

        // Left stick -> movement (only if no keyboard movement)
        if (state.moveX == 0.0f) state.moveX = lx;
        if (state.moveY == 0.0f) state.moveY = -ly; // inverted: stick down = -1

        // Right stick -> look
        float dt = GetFrameTime();
        state.lookX += -rx * stickSens.x * dt;
        state.lookY +=  ry * stickSens.y * dt;

        // A / X button -> jump (Xbox A = BUTTON_RIGHT_FACE_DOWN, PS X = BUTTON_RIGHT_FACE_DOWN)
        if (IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) state.jump = true;

        // Left trigger or B -> crouch
        if (IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_1)) state.crouch = true;
    }

    return state;
}

void InputHandler::SetMouseSensitivity(Vector2 sens) { mouseSens = sens; }
void InputHandler::SetStickSensitivity(Vector2 sens) { stickSens = sens; }
void InputHandler::SetStickDeadzone(float dz)        { deadzone  = dz; }
