#pragma once
#include "raylib.h"
#include "raymath.h"
#include "hot_reload_shader.h"

struct DitherSettings {
    bool  enabled    = true;
    float strength   = 1.0f;
    float colorDepth = 4.0f;
};

struct BarrelSettings {
    bool  enabled  = true;
    float strength = 0.05f;
    float zoom     = 1.2f;
};

struct FogSettings {
    bool  enabled     = false;
    float density     = 0.012f;    // exp² density — 0.02 = light haze, 0.06 = SH2 thick, 0.12 = wall
    float startDist   = 200.0f;     // fog-free zone in front of camera (meters)
    float maxDist     = 5000.0f;   // fully opaque beyond this — used for CPU culling too
    float heightFade  = 1.0f;     // 0 = uniform, >0 = fog thins with altitude (ground fog)
    float ditherBlend = 0.0f;     // how much bayer dither to apply at fog edges (0-1)
    Color color       = {75, 85, 90, 155}; // SH grey
    float nearPlane   = 0.1f;     // must match your camera
    float farPlane    = 5000.0f;   // must match your camera
};

// CPU-side fog culling — call before drawing any obje
inline bool IsFogCulled(const FogSettings& fog, Vector3 camPos, Vector3 objPos, float objRadius = 0.0f) {
    if (!fog.enabled) return false;
    float dist = Vector3Distance(camPos, objPos) - objRadius;
    return dist > fog.maxDist;
}

class PostProcess {
public:
    void Init(int width, int height,
                const char* vsPath = "assets/shaders/postprocess.vs",
                const char* fsPath = "assets/shaders/postprocess.fs");
    void Unload();
    void CheckReload();

    void Begin();
    void End();
    void Resize(int width, int height);

    DitherSettings& GetDither()  { return dither; }
    BarrelSettings& GetBarrel()  { return barrel; }
    FogSettings&    GetFog()     { return fog; }

    // Expose for external fog culling
    const FogSettings& GetFogConst() const { return fog; }

    // Access render target (needed to bind depth texture)
    const RenderTexture2D& GetTarget() const { return target; }

private:
    RenderTexture2D target{};
    HotReloadShader hotReload;
    int width = 0, height = 0;

    DitherSettings dither;
    BarrelSettings barrel;
    FogSettings    fog;

    // Uniform locations
    int resolutionLoc{-1};
    int barrelEnabledLoc{-1}, barrelStrengthLoc{-1}, barrelZoomLoc{-1};
    int ditherEnabledLoc{-1}, ditherStrengthLoc{-1}, ditherColorDepthLoc{-1};
    int fogEnabledLoc{-1}, fogDensityLoc{-1}, fogStartLoc{-1}, fogMaxDistLoc{-1};
    int fogHeightFadeLoc{-1}, fogDitherBlendLoc{-1}, fogColorLoc{-1};
    int fogNearLoc{-1}, fogFarLoc{-1}, depthTexLoc{-1};

    void CacheLocations(Shader s);
};