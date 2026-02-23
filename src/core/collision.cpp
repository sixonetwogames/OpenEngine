#include "collision.h"
#include <cmath>
#include <algorithm>

// --- Registration ---

uint32_t CollisionSystem::AddCollider(const Collider& col) {
    Collider c = col;
    c.tag = (c.tag == 0) ? nextId : c.tag;
    colliders.push_back(c);
    return nextId++;
}

uint32_t CollisionSystem::AddAABB(Vector3 center, Vector3 halfExtents, uint32_t tag, bool trigger) {
    Collider c{};
    c.bounds = {
        {center.x - halfExtents.x, center.y - halfExtents.y, center.z - halfExtents.z},
        {center.x + halfExtents.x, center.y + halfExtents.y, center.z + halfExtents.z}
    };
    c.tag       = tag;
    c.isTrigger = trigger;
    return AddCollider(c);
}

void CollisionSystem::RemoveCollider(uint32_t id) {
    for (size_t i = 0; i < colliders.size(); i++) {
        if (colliders[i].tag == id) {
            colliders.erase(colliders.begin() + i);
            return;
        }
    }
}

void CollisionSystem::Clear() {
    colliders.clear();
    cubeSpawns.clear();
    billboardSpawns.clear();
    nextId = 1;
}

// --- Spawn Helpers ---

uint32_t CollisionSystem::SpawnCube(const MeshSpawn& desc) {
    cubeSpawns.push_back(desc);
    if (!desc.hasCollision) return 0;
    Vector3 half = {desc.size.x * 0.5f, desc.size.y * 0.5f, desc.size.z * 0.5f};
    return AddAABB(desc.position, half, desc.tag, desc.isTrigger);
}

uint32_t CollisionSystem::SpawnBillboard(const BillboardSpawn& desc, Camera camera) {
    billboardSpawns.push_back(desc);
    if (!desc.hasCollision) return 0;
    Vector3 half = {desc.width * 0.5f, desc.height * 0.5f, desc.width * 0.5f};
    return AddAABB(desc.position, half, desc.tag, desc.isTrigger);
}

// --- Drawing ---

void CollisionSystem::DrawCubes() const {
    for (auto& s : cubeSpawns) {
        DrawCubeV(s.position, s.size, s.color);
        if (s.wireColor.a > 0)
            DrawCubeWiresV(s.position, s.size, s.wireColor);
    }
}

void CollisionSystem::DrawBillboards(Camera camera) const {
    for (auto& b : billboardSpawns) {
        DrawBillboard(camera, b.texture, b.position, b.width, b.tint);
    }
}

// --- Collision Resolution ---

bool CollisionSystem::CylinderAABBOverlap(Vector3 pos, float radius, float height, const AABB& box) const {
    float pBot = pos.y;
    float pTop = pos.y + height;
    if (pTop < box.min.y || pBot > box.max.y) return false;

    float closestX = std::clamp(pos.x, box.min.x, box.max.x);
    float closestZ = std::clamp(pos.z, box.min.z, box.max.z);
    float dx = pos.x - closestX;
    float dz = pos.z - closestZ;
    return (dx * dx + dz * dz) <= (radius * radius);
}

Vector3 CollisionSystem::CylinderAABBPushout(Vector3 pos, float radius, float height, const AABB& box) const {
    float closestX = std::clamp(pos.x, box.min.x, box.max.x);
    float closestZ = std::clamp(pos.z, box.min.z, box.max.z);

    float dx = pos.x - closestX;
    float dz = pos.z - closestZ;
    float dist = sqrtf(dx * dx + dz * dz);

    Vector3 pushout = {0};
    constexpr float MAX_PUSHOUT = 2.0f;

    if (dist > 0.0001f) {
        float penetration = radius - dist;
        pushout.x = (dx / dist) * penetration;
        pushout.z = (dz / dist) * penetration;
    } else {
        float pushXPos = box.max.x - pos.x + radius;
        float pushXNeg = pos.x - box.min.x + radius;
        float pushZPos = box.max.z - pos.z + radius;
        float pushZNeg = pos.z - box.min.z + radius;

        float minPush = std::min({pushXPos, pushXNeg, pushZPos, pushZNeg});
        if (minPush == pushXPos)      pushout.x =  pushXPos;
        else if (minPush == pushXNeg) pushout.x = -pushXNeg;
        else if (minPush == pushZPos) pushout.z =  pushZPos;
        else                          pushout.z = -pushZNeg;
    }

    pushout.x = std::clamp(pushout.x, -MAX_PUSHOUT, MAX_PUSHOUT);
    pushout.z = std::clamp(pushout.z, -MAX_PUSHOUT, MAX_PUSHOUT);
    return pushout;
}

bool CollisionSystem::ResolveBody(Vector3& position, Vector3 prevPosition, float radius, float height) const {
    bool collided = false;
    constexpr int MAX_ITERATIONS = 4;

    for (int iter = 0; iter < MAX_ITERATIONS; iter++) {
        bool anyHit = false;
        for (auto& col : colliders) {
            if (col.isTrigger) continue;
            if (!CylinderAABBOverlap(position, radius, height, col.bounds)) continue;

            Vector3 push = CylinderAABBPushout(position, radius, height, col.bounds);

            if (push.x != push.x || push.z != push.z ||
                std::abs(push.x) > 100.0f || std::abs(push.z) > 100.0f) {
                position = prevPosition;
                return true;
            }

            position.x += push.x;
            position.z += push.z;
            anyHit = true;
            collided = true;
        }
        if (!anyHit) break;
    }

    if (position.x != position.x || position.z != position.z) {
        position = prevPosition;
    }

    return collided;
}

bool CollisionSystem::CheckGround(Body& body, float groundY) {
    if (body.position.y <= groundY) {
        body.position.y = groundY;
        body.velocity.y = 0.0f;
        body.isGrounded = true;
        return true;
    }
    body.isGrounded = false;
    return false;
}

// --- Queries ---

bool CollisionSystem::Overlaps(const AABB& a, const AABB& b) const {
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

std::vector<uint32_t> CollisionSystem::QueryOverlaps(Vector3 center, float radius) const {
    std::vector<uint32_t> results;
    for (auto& col : colliders) {
        if (CylinderAABBOverlap(center, radius, 2.0f, col.bounds))
            results.push_back(col.tag);
    }
    return results;
}