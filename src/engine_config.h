#pragma once

namespace EngineConfig {

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
inline constexpr int   SCREEN_W         = 1920;
inline constexpr int   SCREEN_H         = 1080;
inline constexpr int   TARGET_FPS       = 60;
inline constexpr bool  START_FULLSCREEN = false;

// ---------------------------------------------------------------------------
// Player / Camera
// ---------------------------------------------------------------------------
inline constexpr float PLAYER_HEIGHT      = 1.8f;
inline constexpr float PLAYER_SPEED       = 5.0f;
inline constexpr float PLAYER_SPRINT_MULT = 2.0f;
inline constexpr float MOUSE_SENSITIVITY  = 0.002f;
inline constexpr float FOV_DEFAULT        = 90.0f;

// ---------------------------------------------------------------------------
// Player body
// ---------------------------------------------------------------------------
inline constexpr float STAND_HEIGHT    = 1.6f;
inline constexpr float CROUCH_HEIGHT   = 0.9f;
inline constexpr float BOTTOM_HEIGHT   = 0.15f;
inline constexpr float GRAVITY         = 20.0f;
inline constexpr float JUMP_FORCE      = 7.0f;
inline constexpr float MAX_SPEED       = 5.0f;
inline constexpr float CROUCH_SPEED    = 2.5f;
inline constexpr float MAX_ACCEL       = 50.0f;
inline constexpr float FRICTION        = 0.85f;
inline constexpr float AIR_DRAG        = 0.98f;
inline constexpr float CONTROL_RESP    = 15.0f;

// ---------------------------------------------------------------------------
// Slide
// ---------------------------------------------------------------------------
inline constexpr float SLIDE_MIN_SPEED  = 3.0f;
inline constexpr float SLIDE_EXIT_SPEED = 1.5f;
inline constexpr float SLIDE_BOOST      = 1.3f;
inline constexpr float SLIDE_FRICTION   = 0.98f;
inline constexpr float SLIDE_STEER      = 3.0f;
inline constexpr float SLIDE_FOV        = 100.0f;
inline constexpr float SPRINT_FOV       = 95.0f;

// ---------------------------------------------------------------------------
// Camera bob — walk
// ---------------------------------------------------------------------------
inline constexpr float BOB_FREQ       = 2.0f;
inline constexpr float BOB_SIDE       = 0.08f;
inline constexpr float BOB_UP         = 0.06f;
inline constexpr float STEP_ROTATION  = 0.012f;

// ---------------------------------------------------------------------------
// Camera bob — idle (breathing / micro-sway)
// ---------------------------------------------------------------------------
inline constexpr float IDLE_BOB_FREQ    = 0.2f;
inline constexpr float IDLE_BOB_SIDE    = 0.01f;
inline constexpr float IDLE_BOB_UP      = 0.03f;
inline constexpr float IDLE_BOB_ROLL    = 0.008f;

// ---------------------------------------------------------------------------
// Camera lean
// ---------------------------------------------------------------------------
inline constexpr float LEAN_SIDE_MAX  = 0.02f;
inline constexpr float LEAN_FWD_MAX   = 0.015f;
inline constexpr float LEAN_RATE      = 10.0f;

// ---------------------------------------------------------------------------
// Camera lerp rates
// ---------------------------------------------------------------------------
inline constexpr float HEAD_LERP_RATE   = 20.0f;
inline constexpr float WALK_LERP_RATE   = 10.0f;
inline constexpr float FOV_LERP_MOVE    = 5.0f;
inline constexpr float FOV_LERP_SLIDE   = 8.0f;

// ---------------------------------------------------------------------------
// Assets (paths)
// ---------------------------------------------------------------------------
inline constexpr const char* DEFAULT_LEVEL  = "assets/levels/demolevel.json";

} // namespace EngineConfig