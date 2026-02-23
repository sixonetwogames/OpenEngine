#pragma once
#include "types.h"
#include "collision.h"

class PlayerController {
public:
    void Init(Vector3 startPos = {0});
    void Update(const InputState& input, const CollisionSystem* collision = nullptr,
                float collisionRadius = 0.3f, float collisionHeight = 1.8f);
    Camera GetCamera() const;

    const Body& GetBody() const { return body; }
    bool IsSliding() const { return isSliding; }
    Vector2 GetLookRotation() const { return lookRotation; }

    void SetSlideBoost(float boost) { slideBoost = boost; }
    float GetSlideBoost() const { return slideBoost; }

private:
    Body    body       = {};
    Vector2 lookRotation = {0};
    float   headTimer  = 0.0f;
    float   walkLerp   = 0.0f;
    float   headLerp   = STAND_HEIGHT;
    Vector2 lean       = {0};
    float   fov        = DEFAULT_FOV;
    bool    isSliding  = false;
    Vector3 slideDir   = {};
    float   slideBoost = SLIDE_BOOST; // configurable at runtime

    void UpdateBody(const InputState& input);
};