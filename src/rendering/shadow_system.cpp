#include "shadow_system.h"
#include "rlgl.h"
#include <cmath>

void ShadowSystem::CacheLocations(Shader s) {
    s.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(s, "mvp");
    opacityLoc = GetShaderLocation(s, "opacity");
    colorLoc   = GetShaderLocation(s, "color");
    quadModel.materials[0].shader = s;
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

void ShadowSystem::CheckReload() {
    hotReload.Poll();
}

void ShadowSystem::Draw(const std::vector<ShadowCaster>& casters) const {
    if (!settings.enabled) return;

    Shader s = hotReload.Get();
    float sunElev = -sunDir.y;
    if (sunElev < 0.01f) return;

    float sunFade = fminf(sunElev / 0.25f, 1.0f);
    float op = settings.opacity * sunFade;

    SetShaderValue(s, colorLoc,   &settings.color, SHADER_UNIFORM_VEC3);
    SetShaderValue(s, opacityLoc, &op,             SHADER_UNIFORM_FLOAT);

    // Enable alpha blending for smooth shadow edges
    rlEnableColorBlend();
    rlSetBlendMode(RL_BLEND_ALPHA);
    rlDisableDepthMask();

    float hLen = sqrtf(sunDir.x * sunDir.x + sunDir.z * sunDir.z);
    float hx = (hLen > 0.001f) ? sunDir.x / hLen : 0.0f;
    float hz = (hLen > 0.001f) ? sunDir.z / hLen : 0.0f;

    float offsetScale = settings.sunOffset * (1.0f / fmaxf(sunElev, 0.1f));
    offsetScale = fminf(offsetScale, settings.sunOffset * 8.0f);

    // Stretch along sun direction when low — 1.0 at noon, up to maxStretch at horizon
    float stretch = 1.0f + (1.0f - fminf(sunElev / 0.5f, 1.0f)) * (settings.maxStretch - 1.0f);

    constexpr float pad = 1.5f;

    for (const auto& c : casters) {
        float halfH = c.size.y * 0.5f;
        float offAmount = halfH * offsetScale;

        Vector3 pos = {
            c.position.x + hx * offAmount,
            0.015f,
            c.position.z + hz * offAmount,
        };

        // Base size from caster footprint
        float sx = c.size.x * settings.sizeScale * pad;
        float sz = c.size.z * settings.sizeScale * pad;

        // Stretch along sun horizontal direction
        // hx,hz is unit vector — add stretch component to each axis proportionally
        Vector3 scale = {
            sx + fabsf(hx) * sx * (stretch - 1.0f),
            1.0f,
            sz + fabsf(hz) * sz * (stretch - 1.0f),
        };

        DrawModelEx(quadModel, pos, {0, 1, 0}, 0.0f, scale, WHITE);
    }

    rlEnableDepthMask();
}