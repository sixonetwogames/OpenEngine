#include "player_controller.h"
#include <cmath>

void PlayerController::Init(Vector3 startPos) {
    body = {};
    body.position = startPos;
    lookRotation = {0};
    headTimer = 0.0f;
    walkLerp  = 0.0f;
    headLerp  = STAND_HEIGHT;
    lean      = {0};
    fov       = DEFAULT_FOV;
    isSliding = false;
    slideDir  = {0};
}

void PlayerController::Update(const InputState& input, const CollisionSystem* collision,
                               float collisionRadius, float collisionHeight) {
    lookRotation.x += input.lookX;
    lookRotation.y += input.lookY;

    Vector3 prevPos = body.position;
    UpdateBody(input);

    if (collision) {
        collision->ResolveBody(body.position, prevPos, collisionRadius, collisionHeight);
    }

    CollisionSystem::CheckGround(body);

    float dt = GetFrameTime();
    headLerp = Lerp(headLerp, input.crouch ? CROUCH_HEIGHT : STAND_HEIGHT, 20.0f * dt);

    bool moving = body.isGrounded && (input.moveX != 0.0f || input.moveY != 0.0f);
    if (isSliding) {
        walkLerp = Lerp(walkLerp, 0.0f, 10.0f * dt);
        fov      = Lerp(fov, SLIDE_FOV, 8.0f * dt);
    } else if (moving) {
        headTimer += dt * 3.0f;
        walkLerp = Lerp(walkLerp, 1.0f, 10.0f * dt);
        fov      = Lerp(fov, SPRINT_FOV, 5.0f * dt);
    } else {
        walkLerp = Lerp(walkLerp, 0.0f, 10.0f * dt);
        fov      = Lerp(fov, DEFAULT_FOV, 5.0f * dt);
    }

    float sideSign = (input.moveX > 0.0f) - (input.moveX < 0.0f);
    float fwdSign  = (input.moveY > 0.0f) - (input.moveY < 0.0f);
    lean.x = Lerp(lean.x, sideSign * 0.02f, 10.0f * dt);
    lean.y = Lerp(lean.y, fwdSign  * 0.015f, 10.0f * dt);
}

Camera PlayerController::GetCamera() const {
    Camera camera = {0};
    camera.fovy       = fov;
    camera.projection = CAMERA_PERSPECTIVE;
    camera.position   = {
        body.position.x,
        body.position.y + BOTTOM_HEIGHT + headLerp,
        body.position.z
    };

    const Vector3 up = {0, 1, 0};
    const Vector3 targetOffset = {0, 0, -1};

    Vector3 yaw   = Vector3RotateByAxisAngle(targetOffset, up, lookRotation.x);
    Vector3 right = Vector3Normalize(Vector3CrossProduct(yaw, up));

    float pitchAngle = Clamp(-lookRotation.y - lean.y, -PI/2 + 0.0001f, PI/2 - 0.0001f);
    Vector3 pitch = Vector3RotateByAxisAngle(yaw, right, pitchAngle);

    float headSin = sinf(headTimer * PI);
    float headCos = cosf(headTimer * PI);
    camera.up = Vector3RotateByAxisAngle(up, pitch, headSin * STEP_ROTATION + lean.x);

    Vector3 bobbing = Vector3Scale(right, headSin * BOB_SIDE);
    bobbing.y = fabsf(headCos * BOB_UP);

    camera.position = Vector3Add(camera.position, Vector3Scale(bobbing, walkLerp));
    camera.target   = Vector3Add(camera.position, pitch);

    return camera;
}

void PlayerController::UpdateBody(const InputState& input) {
    float dt = GetFrameTime();

    if (!body.isGrounded) body.velocity.y -= GRAVITY * dt;

    if (body.isGrounded && input.jump) {
        body.velocity.y = JUMP_FORCE;
        body.isGrounded = false;
        isSliding = false;
    }

    float rot = lookRotation.x;
    Vector3 front = {sinf(rot), 0, cosf(rot)};
    Vector3 right = {cosf(-rot), 0, sinf(-rot)};

    float hSpeed = sqrtf(body.velocity.x * body.velocity.x + body.velocity.z * body.velocity.z);

    // --- Slide entry ---
    if (input.crouch && body.isGrounded && !isSliding && hSpeed >= SLIDE_MIN_SPEED) {
        isSliding = true;
        float invSpeed = 1.0f / hSpeed;
        slideDir = {body.velocity.x * invSpeed, 0, body.velocity.z * invSpeed};
        body.velocity.x *= slideBoost;
        body.velocity.z *= slideBoost;
    }

    // --- Slide exit ---
    if (isSliding && (!input.crouch || !body.isGrounded || hSpeed < SLIDE_EXIT_SPEED)) {
        isSliding = false;
    }

    if (isSliding) {
        body.velocity.x *= SLIDE_FRICTION;
        body.velocity.z *= SLIDE_FRICTION;

        Vector3 steer = {
            input.moveX * right.x + (-input.moveY) * front.x,
            0.0f,
            input.moveX * right.z + (-input.moveY) * front.z
        };
        body.velocity.x += steer.x * SLIDE_STEER * dt;
        body.velocity.z += steer.z * SLIDE_STEER * dt;
    } else {
        Vector3 desiredDir = {
            input.moveX * right.x + (-input.moveY) * front.x,
            0.0f,
            input.moveX * right.z + (-input.moveY) * front.z
        };
        body.dir = Vector3Lerp(body.dir, desiredDir, CONTROL_RESP * dt);

        float decel = body.isGrounded ? FRICTION : AIR_DRAG;
        Vector3 hvel = {body.velocity.x * decel, 0, body.velocity.z * decel};

        if (Vector3Length(hvel) < MAX_SPEED * 0.01f) hvel = {0};

        float speed    = Vector3DotProduct(hvel, body.dir);
        float maxSpeed = input.crouch ? CROUCH_SPEED : MAX_SPEED;
        float accel    = Clamp(maxSpeed - speed, 0.0f, MAX_ACCEL * dt);
        hvel.x += body.dir.x * accel;
        hvel.z += body.dir.z * accel;

        body.velocity.x = hvel.x;
        body.velocity.z = hvel.z;
    }

    // Clamp horizontal speed
    float finalHSpeed = sqrtf(body.velocity.x * body.velocity.x + body.velocity.z * body.velocity.z);
    constexpr float MAX_H_SPEED = MAX_SPEED * 2.0f;
    if (finalHSpeed > MAX_H_SPEED) {
        float scale = MAX_H_SPEED / finalHSpeed;
        body.velocity.x *= scale;
        body.velocity.z *= scale;
    }

    // NaN guard
    if (body.velocity.x != body.velocity.x || body.velocity.z != body.velocity.z) {
        body.velocity = {0};
    }

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