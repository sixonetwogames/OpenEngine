#pragma once
#include "raylib.h"
#include "raymath.h"
#include "hot_reload_shader.h"
#include "shader_utils.h"
#include "shadow_system.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

// ─── Sprite sheet layout ────────────────────────────────────────────────────

struct SpriteSheet {
    int   cols        = 1;
    int   rows        = 1;
    int   totalFrames = 1;
    float fps         = 0.0f;
    bool  loop        = true;
};

// ─── Billboard definition (template) ────────────────────────────────────────

struct BillboardDef {
    Texture2D texture{};
    Vector2   size{1.0f, 1.0f};
    bool      lockY = true;

    float     alphaThresh = 0.1f;
    Color     tint = WHITE;

    // Fake PBR
    float     roughness      = 0.8f;
    float     metallic       = 0.0f;
    float     normalStrength = 0.0f;

    SpriteSheet sheet;

    // Wind
    bool      windEnabled = false;

    // Spherical: faces camera on all axes, enables sphere projection shader
    bool      spherical   = false;
    float     sphereSpeed = 1.0f;   // rotation speed (radians/sec)

    // Shadow defaults for instances using this def
    bool      castsShadow      = true;
    Vector3   shadowSize       = {0.0f, 0.0f, 0.0f}; // {0,0,0} = auto from billboard size
    float     shadowSunOffset  = -1.0f;                // <0 = use global
};

// ─── Billboard instance (runtime) ───────────────────────────────────────────

struct BillboardInstance {
    uint16_t defIndex;
    Vector3  position;
    float    scale         = 1.0f;
    Color    tintOverride  = {0,0,0,0};
    int      frameOverride = -1;

    int8_t   shadowOverride = 0; //  0 = use def,  1 = force on, -1 = force off
};

// ─── BillboardSystem ────────────────────────────────────────────────────────

class BillboardSystem {
public:
    void Init();
    void Init(const char* vsPath, const char* fsPath);
    void Unload();
    void CheckReload();

    // --- Named registry ---
    uint16_t RegisterDef(const std::string& name, const BillboardDef& def);
    uint16_t LookupDef(const std::string& name) const;
    const BillboardDef& GetDef(uint16_t idx) const { return defs[idx]; }
    bool HasDef(const std::string& name) const { return defNames.count(name); }

    // --- Instance management ---
    size_t   Spawn(const BillboardInstance& inst);
    void     SpawnBatch(const BillboardInstance* insts, size_t count);
    void     RemoveInstance(size_t idx);
    void     ClearInstances();
    size_t   InstanceCount() const { return instances.size(); }
    BillboardInstance&       GetInstance(size_t i)       { return instances[i]; }
    const BillboardInstance& GetInstance(size_t i) const { return instances[i]; }

    // --- Per-frame ---
    void Update(float dt);
    void Draw(Camera camera);

    // --- Shadow integration ---
    void GatherShadowCasters(std::vector<ShadowCaster>& out) const;

private:
    std::vector<BillboardDef>      defs;
    std::vector<BillboardInstance> instances;
    std::vector<float>             animTimers;
    std::unordered_map<std::string, uint16_t> defNames;
    std::vector<uint32_t>          drawOrder;

    HotReloadShader hotReload;
    Mesh     quadMesh{};
    Material quadMat{};

    int locTime            = -1;
    int locWind            = -1;
    int locWindDirection   = -1;
    int locWindEnabled     = -1;
    int locBillboardPos    = -1;
    int locBillboardSize   = -1;
    int locAlphaThreshold  = -1;
    int locTintColor       = -1;
    int locUvMin           = -1;
    int locUvMax           = -1;
    int locLockY           = -1;
    int locSpherical       = -1;
    int locSphereSpeed     = -1;
    int locFogNear         = -1;
    int locFogFar          = -1;
    int locFogColor        = -1;
    int locRoughness       = -1;
    int locMetallic        = -1;
    int locNormalStrength  = -1;
    int locSunDir          = -1;
    int locSunColor        = -1;
    int locAmbientColor    = -1;
    int locCameraPos       = -1;

    void CacheLocations(Shader s);
    void SortBackToFront(Vector3 camPos);
    static void ComputeFrameUV(const SpriteSheet& sheet, int frame, Vector2& outMin, Vector2& outMax);
};