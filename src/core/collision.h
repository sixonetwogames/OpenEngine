#pragma once
#include "types.h"
#include <vector>
#include <cstdint>

// --- Collider types ---

struct AABB {
    Vector3 min;
    Vector3 max;
};

struct Collider {
    AABB     bounds;
    uint32_t tag;
    bool     isTrigger;
};

// --- Spawn descriptors ---

struct MeshSpawn {
    Vector3  position;
    Vector3  size;
    Color    color;
    Color    wireColor;
    uint32_t tag         = 0;
    bool     isTrigger   = false;
    bool     hasCollision = true;
};

struct BillboardSpawn {
    Texture2D texture;
    Vector3   position;
    float     width;
    float     height;
    Color     tint;
    uint32_t  tag         = 0;
    bool      isTrigger   = false;
    bool      hasCollision = true;
};

// --- Collision System ---

class CollisionSystem {
public:
    // Registration
    uint32_t AddCollider(const Collider& col);
    uint32_t AddAABB(Vector3 center, Vector3 halfExtents, uint32_t tag = 0, bool trigger = false);
    void     RemoveCollider(uint32_t id);
    void     Clear();

    // Spawn helpers: visual + collider atomically
    uint32_t SpawnCube(const MeshSpawn& desc);
    uint32_t SpawnBillboard(const BillboardSpawn& desc, Camera camera);

    // Drawing (call inside BeginMode3D)
    void DrawCubes() const;
    void DrawBillboards(Camera camera) const;

    // Resolution
    bool ResolveBody(Vector3& position, Vector3 prevPosition, float radius, float height) const;
    static bool CheckGround(Body& body, float groundY = 0.0f);

    // Queries
    bool                        Overlaps(const AABB& a, const AABB& b) const;
    std::vector<uint32_t>       QueryOverlaps(Vector3 center, float radius) const;
    const std::vector<Collider>& GetColliders() const { return colliders; }

private:
    std::vector<Collider>       colliders;
    std::vector<MeshSpawn>      cubeSpawns;
    std::vector<BillboardSpawn> billboardSpawns;
    uint32_t nextId = 1;

    bool    CylinderAABBOverlap(Vector3 pos, float radius, float height, const AABB& box) const;
    Vector3 CylinderAABBPushout(Vector3 pos, float radius, float height, const AABB& box) const;
};