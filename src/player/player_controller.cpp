#include "player_controller.h"
#include <cmath>

namespace EC = EngineConfig;

void PlayerController::Init(Vector3 startPos) {
    body = {};
    body.position  = startPos;
    lookRotation   = {0};
    headTimer      = 0.0f;
    idleTimer      = 0.0f;
    walkLerp       = 0.0f;
    idleLerp       = 0.0f;
    headLerp       = EC::STAND_HEIGHT;
    lean           = {0};
    fov            = EC::FOV_DEFAULT;
    isSliding      = false;
    slideDir       = {0};
    bobOffset      = {0};
    bobRoll        = 0.0f;
}

void PlayerController::Update(const InputState& input, const CollisionSystem* collision,
                               float collisionRadius, float collisionHeight) {
    lookRotation.x += input.lookX;
    lookRotation.y += input.lookY;

    Vector3 prevPos = body.position;
    UpdateBody(input);

    if (collision)
        collision->ResolveBody(body.position, prevPos, collisionRadius, collisionHeight);

    CollisionSystem::CheckGround(body);

    float dt = GetFrameTime();
    headLerp = Lerp(headLerp, input.crouch ? EC::CROUCH_HEIGHT : EC::STAND_HEIGHT,
                    EC::HEAD_LERP_RATE * dt);

    bool moving = body.isGrounded && (input.moveX != 0.0f || input.moveY != 0.0f);

    if (isSliding) {
        walkLerp = Lerp(walkLerp, 0.0f, EC::WALK_LERP_RATE * dt);
        idleLerp = Lerp(idleLerp, 0.0f, EC::WALK_LERP_RATE * dt);
        fov      = Lerp(fov, EC::SLIDE_FOV, EC::FOV_LERP_SLIDE * dt);
    } else if (moving) {
        headTimer += dt * EC::BOB_FREQ;
        walkLerp   = Lerp(walkLerp, 1.0f, EC::WALK_LERP_RATE * dt);
        idleLerp   = Lerp(idleLerp, 0.0f, EC::WALK_LERP_RATE * dt);
        fov        = Lerp(fov, EC::SPRINT_FOV, EC::FOV_LERP_MOVE * dt);
    } else {
        idleTimer += dt * EC::IDLE_BOB_FREQ;
        walkLerp   = Lerp(walkLerp, 0.0f, EC::WALK_LERP_RATE * dt);
        idleLerp   = Lerp(idleLerp, 1.0f, EC::WALK_LERP_RATE * dt);
        fov        = Lerp(fov, EC::FOV_DEFAULT, EC::FOV_LERP_MOVE * dt);
    }

    float sideSign = (input.moveX > 0.0f) - (input.moveX < 0.0f);
    float fwdSign  = (input.moveY > 0.0f) - (input.moveY < 0.0f);
    lean.x = Lerp(lean.x, sideSign * EC::LEAN_SIDE_MAX, EC::LEAN_RATE * dt);
    lean.y = Lerp(lean.y, fwdSign  * EC::LEAN_FWD_MAX,  EC::LEAN_RATE * dt);

    UpdateBob(input, dt);
}

// ---------------------------------------------------------------------------
// Consolidated bob: walk + idle + lean → single offset & roll per frame
// ---------------------------------------------------------------------------
void PlayerController::UpdateBob(const InputState& /*input*/, float /*dt*/) {
    float walkSin = sinf(headTimer * PI);
    float walkCos = cosf(headTimer * PI);
    float idleSin = sinf(idleTimer * PI * 2.0f);        // full cycle
    float idleCos = cosf(idleTimer * PI * 2.0f * 0.7f); // slightly detuned for organic feel

    // Walk bob (scaled by walkLerp)
    float wSide = walkSin * EC::BOB_SIDE * walkLerp;
    float wUp   = fabsf(walkCos * EC::BOB_UP) * walkLerp;
    float wRoll = walkSin * EC::STEP_ROTATION * walkLerp;

    // Idle bob (scaled by idleLerp)
    float iSide = idleSin * EC::IDLE_BOB_SIDE * idleLerp;
    float iUp   = idleCos * EC::IDLE_BOB_UP   * idleLerp;
    float iRoll = idleSin * EC::IDLE_BOB_ROLL  * idleLerp;

    // Accumulate into single vectors
    bobOffset = {wSide + iSide, wUp + iUp, 0.0f};
    bobRoll   = wRoll + iRoll + lean.x;
}

Camera PlayerController::GetCamera() const {
    Camera camera  = {0};
    camera.fovy    = fov;
    camera.projection = CAMERA_PERSPECTIVE;
    camera.position = {
        body.position.x,
        body.position.y + EC::BOTTOM_HEIGHT + headLerp,
        body.position.z
    };

    const Vector3 up = {0, 1, 0};
    const Vector3 fwd = {0, 0, -1};

    Vector3 yaw   = Vector3RotateByAxisAngle(fwd, up, lookRotation.x);
    Vector3 right = Vector3Normalize(Vector3CrossProduct(yaw, up));

    float pitchAngle = Clamp(-lookRotation.y - lean.y, -PI / 2 + 0.0001f, PI / 2 - 0.0001f);
    Vector3 pitch = Vector3RotateByAxisAngle(yaw, right, pitchAngle);

    // Apply consolidated bob offset in camera-local space
    camera.position = Vector3Add(camera.position, Vector3Scale(right, bobOffset.x));
    camera.position.y += bobOffset.y;
    // bobOffset.z reserved for forward shake if needed

    // Apply consolidated roll
    camera.up = Vector3RotateByAxisAngle(up, pitch, bobRoll);

    camera.target = Vector3Add(camera.position, pitch);
    return camera;
}

void PlayerController::UpdateBody(const InputState& input) {
    float dt = GetFrameTime();

    if (!body.isGrounded) body.velocity.y -= EC::GRAVITY * dt;

    if (body.isGrounded && input.jump) {
        body.velocity.y = EC::JUMP_FORCE;
        body.isGrounded = false;
        isSliding = false;
    }

    float rot = lookRotation.x;
    Vector3 front = {sinf(rot), 0, cosf(rot)};
    Vector3 right = {cosf(-rot), 0, sinf(-rot)};

    float hSpeed = sqrtf(body.velocity.x * body.velocity.x +
                         body.velocity.z * body.velocity.z);

    // Slide entry
    if (input.crouch && body.isGrounded && !isSliding && hSpeed >= EC::SLIDE_MIN_SPEED) {
        isSliding = true;
        float invSpeed = 1.0f / hSpeed;
        slideDir = {body.velocity.x * invSpeed, 0, body.velocity.z * invSpeed};
        body.velocity.x *= slideBoost;
        body.velocity.z *= slideBoost;
    }

    // Slide exit
    if (isSliding && (!input.crouch || !body.isGrounded || hSpeed < EC::SLIDE_EXIT_SPEED))
        isSliding = false;

    if (isSliding) {
        body.velocity.x *= EC::SLIDE_FRICTION;
        body.velocity.z *= EC::SLIDE_FRICTION;

        Vector3 steer = {
            input.moveX * right.x + (-input.moveY) * front.x, 0.0f,
            input.moveX * right.z + (-input.moveY) * front.z
        };
        body.velocity.x += steer.x * EC::SLIDE_STEER * dt;
        body.velocity.z += steer.z * EC::SLIDE_STEER * dt;
    } else {
        Vector3 desiredDir = {
            input.moveX * right.x + (-input.moveY) * front.x, 0.0f,
            input.moveX * right.z + (-input.moveY) * front.z
        };
        body.dir = Vector3Lerp(body.dir, desiredDir, EC::CONTROL_RESP * dt);

        float decel = body.isGrounded ? EC::FRICTION : EC::AIR_DRAG;
        Vector3 hvel = {body.velocity.x * decel, 0, body.velocity.z * decel};

        if (Vector3Length(hvel) < EC::MAX_SPEED * 0.01f) hvel = {0};

        float speed    = Vector3DotProduct(hvel, body.dir);
        float maxSpeed = input.crouch ? EC::CROUCH_SPEED : EC::MAX_SPEED;
        float accel    = Clamp(maxSpeed - speed, 0.0f, EC::MAX_ACCEL * dt);
        hvel.x += body.dir.x * accel;
        hvel.z += body.dir.z * accel;

        body.velocity.x = hvel.x;
        body.velocity.z = hvel.z;
    }

    // Clamp horizontal speed
    float finalHSpeed = sqrtf(body.velocity.x * body.velocity.x +
                              body.velocity.z * body.velocity.z);
    constexpr float MAX_H_SPEED = EC::MAX_SPEED * 2.0f;
    if (finalHSpeed > MAX_H_SPEED) {
        float scale = MAX_H_SPEED / finalHSpeed;
        body.velocity.x *= scale;
        body.velocity.z *= scale;
    }

    // NaN guard
    if (body.velocity.x != body.velocity.x || body.velocity.z != body.velocity.z)
        body.velocity = {0};

    // Limit per-frame displacement
    Vector3 delta = {body.velocity.x * dt, body.velocity.y * dt, body.velocity.z * dt};
    constexpr float MAX_STEP = 1.0f;
    float hDelta = sqrtf(delta.x * delta.x + delta.z * delta.z);
    if (hDelta > MAX_STEP) {
        float scale = MAX_STEP / hDelta;
        delta.x *= scale;
        delta.z *= scale;
    }

    body.position.x += delta.x;
    body.position.y += delta.y;
    body.position.z += delta.z;
}