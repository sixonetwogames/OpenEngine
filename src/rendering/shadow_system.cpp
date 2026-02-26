#include "shadow_system.h"
#include "shader_utils.h"
#include "rlgl.h"
#include <cmath>

void ShadowSystem::CacheLocations(Shader s) {
    s.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(s, "mvp");
    opacityLoc = GetShaderLocation(s, "opacity");
    colorLoc   = GetShaderLocation(s, "color");
    quadModel.materials[0].shader = s;
}

void ShadowSystem::Init() {
    std::string vs = std::string(GetShaderPath()) + "shadow.vs";
    std::string fs = std::string(GetShaderPath()) + "shadow.fs";
    Init(vs.c_str(), fs.c_str());
}

void ShadowSystem::Init(const char* vsPath, const char* fsPath) {
    Mesh mesh = GenMeshPlane(1.0f, 1.0f, 1, 1);
    quadModel = LoadModelFromMesh(mesh);
    hotReload.Init(vsPath, fsPath, [this](Shader s) { CacheLocations(s); });
}

void ShadowSystem::Unload() {
    UnloadModel(quadModel);
    hotReload.Unload();
}

void ShadowSystem::CheckReload() { hotReload.Poll(); }

void ShadowSystem::Draw(const std::vector<ShadowCaster>& casters) const {
    if (!World::shadowEnabled) return;

    Shader s = hotReload.Get();
    float sunElev = -sunDir.y;
    if (sunElev < 0.01f) return;

    float sunFade = fminf(sunElev / 0.25f, 1.0f);
    float baseOp  = World::shadowOpacity * sunFade;

    SetShaderValue(s, colorLoc, &World::shadowColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(s, opacityLoc, &baseOp, SHADER_UNIFORM_FLOAT);

    rlEnableColorBlend();
    rlSetBlendFactors(0x0302, 0x0303, 0x8006);
    rlSetBlendMode(BLEND_CUSTOM);
    rlColorMask(true, true, true, false);
    rlDisableDepthMask();

    float hLen = sqrtf(sunDir.x * sunDir.x + sunDir.z * sunDir.z);
    float hx = (hLen > 0.001f) ? sunDir.x / hLen : 0.0f;
    float hz = (hLen > 0.001f) ? sunDir.z / hLen : 0.0f;

    float globalOffsetScale = World::shadowSunOffset * (1.0f / fmaxf(sunElev, 0.1f));
    globalOffsetScale = fminf(globalOffsetScale, World::shadowSunOffset * 8.0f);

    float stretch = 1.0f + (1.0f - fminf(sunElev / 0.5f, 1.0f)) * (World::shadowMaxStretch - 1.0f);
    constexpr float pad = 1.5f;

    for (const auto& c : casters) {
        if (!c.enabled) continue;

        // Per-caster sun offset override
        float offScale = globalOffsetScale;
        if (c.sunOffsetOverride >= 0.0f) {
            offScale = c.sunOffsetOverride * (1.0f / fmaxf(sunElev, 0.1f));
            offScale = fminf(offScale, c.sunOffsetOverride * 8.0f);
        }

        float halfH = c.size.y * 0.5f;
        float offAmount = halfH * offScale;

        Vector3 pos = {
            c.position.x + hx * offAmount,
            0.015f,
            c.position.z + hz * offAmount,
        };

        float sx = c.size.x * World::shadowSizeScale * pad;
        float sz = c.size.z * World::shadowSizeScale * pad;

        Vector3 scale = {
            sx + fabsf(hx) * sx * (stretch - 1.0f),
            1.0f,
            sz + fabsf(hz) * sz * (stretch - 1.0f),
        };

        DrawModelEx(quadModel, pos, {0, 1, 0}, 0.0f, scale, WHITE);
    }

    rlEnableDepthMask();
    rlColorMask(true, true, true, true);
    rlSetBlendMode(BLEND_ALPHA);
    rlDisableColorBlend();
}