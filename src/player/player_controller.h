#pragma once
#include "types.h"
#include "collision.h"
#include "engine_config.h"

namespace EC = EngineConfig;

class PlayerController {
public:
    void Init(Vector3 startPos = {0});
    void Update(const InputState& input, const CollisionSystem* collision = nullptr,
                float collisionRadius = 0.3f,
                float collisionHeight = EC::PLAYER_HEIGHT);
    Camera GetCamera() const;

    const Body& GetBody() const { return body; }
    bool IsSliding() const { return isSliding; }
    Vector2 GetLookRotation() const { return lookRotation; }

    void SetSlideBoost(float boost) { slideBoost = boost; }
    float GetSlideBoost() const { return slideBoost; }

private:

    Body    body         = {};
    Vector2 lookRotation = {0};
    float   headTimer    = 0.0f;
    float   idleTimer    = 0.0f;
    float   walkLerp     = 0.0f;
    float   idleLerp     = 0.0f;
    float   headLerp     = EC::STAND_HEIGHT;
    Vector2 lean         = {0};
    float   fov          = EC::FOV_DEFAULT;
    bool    isSliding    = false;
    Vector3 slideDir     = {};
    float   slideBoost   = EC::SLIDE_BOOST;

    // Accumulated per-frame camera offsets (computed in Update, consumed in GetCamera)
    Vector3 bobOffset    = {};   // combined positional offset
    float   bobRoll      = 0.0f; // combined roll angle

    void UpdateBody(const InputState& input);
    void UpdateBob(const InputState& input, float dt);
};