#include "billboard_system.h"
#include "billboard_presets.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include "world.h"

// ─── Init / Unload ──────────────────────────────────────────────────────────

void BillboardSystem::Init() {
    std::string base = GetShaderPath();
    Init((base + "sprite_mask.vs").c_str(), (base + "sprite_mask.fs").c_str());
}

void BillboardSystem::Init(const char* vsPath, const char* fsPath) {
    quadMesh = GenMeshPlane(1.0f, 1.0f, 1, 1);
    quadMat  = LoadMaterialDefault();
    hotReload.Init(vsPath, fsPath, [this](Shader s) { CacheLocations(s); });
    Shader s = hotReload.Get();
    CacheLocations(s);
}

void BillboardSystem::Unload() {
    UnloadMesh(quadMesh);
    UnloadMaterial(quadMat);
    hotReload.Unload();
    defs.clear();
    defNames.clear();
    instances.clear();
    animTimers.clear();
}

void BillboardSystem::CheckReload() { hotReload.Poll(); }

// ─── Cache locations ────────────────────────────────────────────────────────

void BillboardSystem::CacheLocations(Shader s) {
    s.locs[SHADER_LOC_MATRIX_VIEW]       = GetShaderLocation(s, "matView");
    s.locs[SHADER_LOC_MATRIX_PROJECTION] = GetShaderLocation(s, "matProjection");

    locTime            = GetShaderLocation(s, "time");
    locWind            = GetShaderLocation(s, "wind");
    locWindDirection   = GetShaderLocation(s, "windDirection");
    locWindEnabled     = GetShaderLocation(s, "windEnabled");
    locBillboardPos    = GetShaderLocation(s, "billboardPos");
    locBillboardSize   = GetShaderLocation(s, "billboardSize");
    locAlphaThreshold  = GetShaderLocation(s, "alphaThreshold");
    locTintColor       = GetShaderLocation(s, "tintColor");
    locUvMin           = GetShaderLocation(s, "uvMin");
    locUvMax           = GetShaderLocation(s, "uvMax");
    locLockY           = GetShaderLocation(s, "lockY");
    locSpherical       = GetShaderLocation(s, "spherical");
    locSphereSpeed     = GetShaderLocation(s, "sphereSpeed");
    locFogNear         = GetShaderLocation(s, "fogNear");
    locFogFar          = GetShaderLocation(s, "fogFar");
    locFogColor        = GetShaderLocation(s, "fogColor");
    locRoughness       = GetShaderLocation(s, "roughness");
    locMetallic        = GetShaderLocation(s, "metallic");
    locNormalStrength  = GetShaderLocation(s, "normalStrength");
    locSunDir          = GetShaderLocation(s, "sunDir");
    locSunColor        = GetShaderLocation(s, "sunColor");
    locAmbientColor    = GetShaderLocation(s, "ambientColor");
    locCameraPos       = GetShaderLocation(s, "cameraPos");
}

// ─── Registry ───────────────────────────────────────────────────────────────

uint16_t BillboardSystem::RegisterDef(const std::string& name, const BillboardDef& def) {
    uint16_t idx = static_cast<uint16_t>(defs.size());
    defs.push_back(def);
    defNames[name] = idx;
    return idx;
}

uint16_t BillboardSystem::LookupDef(const std::string& name) const {
    auto it = defNames.find(name);
    return (it != defNames.end()) ? it->second : UINT16_MAX;
}

// ─── Instance management ────────────────────────────────────────────────────

size_t BillboardSystem::Spawn(const BillboardInstance& inst) {
    size_t idx = instances.size();
    instances.push_back(inst);
    animTimers.push_back(0.0f);
    return idx;
}

void BillboardSystem::SpawnBatch(const BillboardInstance* insts, size_t count) {
    instances.insert(instances.end(), insts, insts + count);
    animTimers.resize(instances.size(), 0.0f);
}

void BillboardSystem::RemoveInstance(size_t idx) {
    if (idx >= instances.size()) return;
    instances[idx]  = instances.back();  instances.pop_back();
    animTimers[idx] = animTimers.back(); animTimers.pop_back();
}

void BillboardSystem::ClearInstances() {
    instances.clear();
    animTimers.clear();
}

// ─── Animation update ───────────────────────────────────────────────────────

void BillboardSystem::Update(float dt) {
    for (size_t i = 0; i < instances.size(); i++) {
        const BillboardDef& def = defs[instances[i].defIndex];
        if (def.sheet.fps <= 0.0f || instances[i].frameOverride >= 0) continue;

        animTimers[i] += dt;
        float frameDur = 1.0f / def.sheet.fps;
        float totalDur = frameDur * def.sheet.totalFrames;

        if (def.sheet.loop) {
            if (animTimers[i] >= totalDur) animTimers[i] = fmodf(animTimers[i], totalDur);
        } else {
            if (animTimers[i] >= totalDur) animTimers[i] = totalDur - frameDur;
        }
    }
}

// ─── Sprite sheet UV ────────────────────────────────────────────────────────

void BillboardSystem::ComputeFrameUV(const SpriteSheet& sheet, int frame,
                                      Vector2& outMin, Vector2& outMax) {
    float fw = 1.0f / sheet.cols;
    float fh = 1.0f / sheet.rows;
    int col = frame % sheet.cols;
    int row = frame / sheet.cols;
    outMin = { col * fw,       row * fh };
    outMax = { (col + 1) * fw, (row + 1) * fh };
}

// ─── Sort back to front ─────────────────────────────────────────────────────

void BillboardSystem::SortBackToFront(Vector3 camPos) {
    const size_t n = instances.size();
    drawOrder.resize(n);
    for (uint32_t i = 0; i < n; i++) drawOrder[i] = i;

    std::sort(drawOrder.begin(), drawOrder.end(), [&](uint32_t a, uint32_t b) {
        float da = Vector3DistanceSqr(instances[a].position, camPos);
        float db = Vector3DistanceSqr(instances[b].position, camPos);
        return da > db;
    });
}

// ─── Shadow caster generation ───────────────────────────────────────────────

void BillboardSystem::GatherShadowCasters(std::vector<ShadowCaster>& out) const {
    out.reserve(out.size() + instances.size());

    for (const auto& inst : instances) {
        const BillboardDef& def = defs[inst.defIndex];

        bool wantShadow = def.castsShadow;
        if (inst.shadowOverride > 0)       wantShadow = true;
        else if (inst.shadowOverride < 0)  wantShadow = false;
        if (!wantShadow) continue;

        Vector3 sz = def.shadowSize;
        if (sz.x == 0.0f && sz.y == 0.0f && sz.z == 0.0f) {
            float w = def.size.x * inst.scale;
            float h = def.size.y * inst.scale;
            sz = { w, h, w };
        } else {
            sz.x *= inst.scale;
            sz.y *= inst.scale;
            sz.z *= inst.scale;
        }

        out.push_back({
            .position          = inst.position,
            .size              = sz,
            .sunOffsetOverride = def.shadowSunOffset,
        });
    }
}

// ─── Draw ───────────────────────────────────────────────────────────────────

void BillboardSystem::Draw(Camera camera) {
    if (instances.empty()) return;

    static bool logged = false;
    if (!logged) {
        TraceLog(LOG_INFO, "BILLBOARD: Drawing %d instances, shader=%d",
                 (int)instances.size(), hotReload.Get().id);
        logged = true;
    }

    Shader s = hotReload.Get();
    SortBackToFront(camera.position);

    SetShaderValue(s, locTime, &World::worldTime, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, locCameraPos, &camera.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(s, locWind, &World::windStrength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, locWindDirection, &World::windDirection, SHADER_UNIFORM_VEC3);
    SetShaderValue(s, locFogNear,  &World::fogStart,   SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, locFogFar,   &World::fogMaxDist, SHADER_UNIFORM_FLOAT);

    float fogCol[3] = { World::fogColor.r/255.0f, World::fogColor.g/255.0f, World::fogColor.b/255.0f };
    SetShaderValue(s, locFogColor, fogCol, SHADER_UNIFORM_VEC3);

    SetShaderValue(s, locSunDir, &World::lightDir, SHADER_UNIFORM_VEC3);

    Vector3 sunScaled = World::lightColor * World::lightIntensity;
    Vector3 ambScaled = World::skylightColor * World::skylightIntensity;
    SetShaderValue(s, locSunColor,     &sunScaled, SHADER_UNIFORM_VEC3);
    SetShaderValue(s, locAmbientColor, &ambScaled, SHADER_UNIFORM_VEC3);

    quadMat.shader = s;

    rlDisableColorBlend();
    rlDisableBackfaceCulling();

    unsigned int boundTexId = 0;
    Matrix identity = MatrixIdentity();

    for (uint32_t idx : drawOrder) {
        const BillboardInstance& inst = instances[idx];
        const BillboardDef&     def  = defs[inst.defIndex];

        if (def.texture.id != boundTexId) {
            quadMat.maps[MATERIAL_MAP_ALBEDO].texture = def.texture;
            boundTexId = def.texture.id;
        }

        Vector2 uvMin, uvMax;
        if (def.sheet.totalFrames > 1) {
            int frame;
            if (inst.frameOverride >= 0) {
                frame = inst.frameOverride % def.sheet.totalFrames;
            } else {
                float frameDur = 1.0f / def.sheet.fps;
                frame = static_cast<int>(animTimers[idx] / frameDur);
                if (frame >= def.sheet.totalFrames) frame = def.sheet.totalFrames - 1;
            }
            ComputeFrameUV(def.sheet, frame, uvMin, uvMax);
        } else {
            uvMin = {0, 0};
            uvMax = {1, 1};
        }

        float sz[2] = { def.size.x * inst.scale, def.size.y * inst.scale };
        Color  t = (inst.tintOverride.a > 0) ? inst.tintOverride : def.tint;
        float tint[4] = { t.r/255.0f, t.g/255.0f, t.b/255.0f, t.a/255.0f };
        int lockYi  = (def.spherical) ? 0 : (def.lockY ? 1 : 0);
        int sphereI = def.spherical ? 1 : 0;
        int windEn  = def.windEnabled ? 1 : 0;

        SetShaderValue(s, locBillboardPos,    &inst.position,      SHADER_UNIFORM_VEC3);
        SetShaderValue(s, locBillboardSize,   sz,                  SHADER_UNIFORM_VEC2);
        SetShaderValue(s, locAlphaThreshold,  &def.alphaThresh,    SHADER_UNIFORM_FLOAT);
        SetShaderValue(s, locTintColor,       tint,                SHADER_UNIFORM_VEC4);
        SetShaderValue(s, locUvMin,           &uvMin,              SHADER_UNIFORM_VEC2);
        SetShaderValue(s, locUvMax,           &uvMax,              SHADER_UNIFORM_VEC2);
        SetShaderValue(s, locLockY,           &lockYi,             SHADER_UNIFORM_INT);
        SetShaderValue(s, locSpherical,       &sphereI,            SHADER_UNIFORM_INT);
        SetShaderValue(s, locSphereSpeed,     &def.sphereSpeed,    SHADER_UNIFORM_FLOAT);
        SetShaderValue(s, locWindEnabled,     &windEn,             SHADER_UNIFORM_INT);
        SetShaderValue(s, locRoughness,       &def.roughness,      SHADER_UNIFORM_FLOAT);
        SetShaderValue(s, locMetallic,        &def.metallic,       SHADER_UNIFORM_FLOAT);
        SetShaderValue(s, locNormalStrength,  &def.normalStrength, SHADER_UNIFORM_FLOAT);

        DrawMesh(quadMesh, quadMat, identity);
    }

    rlEnableColorBlend();
    rlEnableBackfaceCulling();
}