#pragma once
#include "raylib.h"
#include "raymath.h"

//--- Physics Constants ---
constexpr float GRAVITY       = 32.0f;
constexpr float MAX_SPEED     = 20.0f;
constexpr float CROUCH_SPEED  = 5.0f;
constexpr float JUMP_FORCE    = 12.0f;
constexpr float MAX_ACCEL     = 150.0f;
constexpr float FRICTION      = 0.86f;
constexpr float AIR_DRAG      = 0.98f;
constexpr float CONTROL_RESP  = 15.0f;

//--- Slide Constants ---
constexpr float SLIDE_MIN_SPEED  = 10.0f;  // must be going this fast to trigger slide
constexpr float SLIDE_BOOST      = 1.3f;   // speed multiplier on slide entry
constexpr float SLIDE_FRICTION   = 0.97f;  // much less friction than normal
constexpr float SLIDE_EXIT_SPEED = 4.0f;   // slide ends below this speed
constexpr float SLIDE_FOV        = 52.0f;  // wider FOV during slide
constexpr float SLIDE_STEER      = 3.0f;   // how much you can steer mid-slide

//--- Body Constants ---
constexpr float CROUCH_HEIGHT = 0.0f;
constexpr float STAND_HEIGHT  = 1.0f;
constexpr float BOTTOM_HEIGHT = 0.5f;

//--- Camera Constants ---
constexpr float DEFAULT_FOV   = 60.0f;
constexpr float SPRINT_FOV    = 55.0f;
constexpr float BOB_SIDE      = 0.1f;
constexpr float BOB_UP        = 0.15f;
constexpr float STEP_ROTATION = 0.01f;

//--- Input State (unified keyboard + gamepad) ---
struct InputState {
    float moveX;        // -1 left, +1 right
    float moveY;        // -1 back, +1 forward
    float lookX;        // horizontal look delta
    float lookY;        // vertical look delta
    bool jump;          // pressed this frame
    bool crouch;        // held
};

//--- Physics Body ---
struct Body {
    Vector3 position = {};
    Vector3 velocity = {};
    Vector3 dir      = {};
    bool isGrounded  = false;
};

//--- Collision Map ---
struct CollisionMap {
    Color*  pixels  = nullptr;
    int     width   = 0;
    int     height  = 0;
    Vector3 offset  = {};  // world-space offset of map origin
};