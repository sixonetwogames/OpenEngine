#pragma once
#include "raylib.h"
#include "raymath.h"
#include "hot_reload_shader.h"

struct DitherSettings {
    bool  enabled    = false;
    float strength   = 0.8f;
    float colorDepth = 4.0f;
};

struct BarrelSettings {
    bool  enabled  = false;
    float strength = 0.1f;
    float zoom     = 1.2f;
};

class PostProcess {
public:
    void Init(int w, int h, int sw, int sh);
    void Init(int w, int h, int sw, int sh, const char* vsPath, const char* fsPath);
    void Unload();
    void CheckReload();

    void Begin();
    void End();
    void Resize(int width, int height);

    DitherSettings& GetDither() { return dither; }
    BarrelSettings& GetBarrel() { return barrel; }

    const RenderTexture2D& GetTarget() const { return target; }

private:
    RenderTexture2D target{};
    HotReloadShader hotReload;
    int width = 0, height = 0;

    DitherSettings dither;
    BarrelSettings barrel;

    // Uniform locations
    int screenW, screenH;
    int resolutionLoc{-1};
    int barrelEnabledLoc{-1}, barrelStrengthLoc{-1}, barrelZoomLoc{-1};
    int ditherEnabledLoc{-1}, ditherStrengthLoc{-1}, ditherColorDepthLoc{-1};
    int fogEnabledLoc{-1}, fogDensityLoc{-1}, fogStartLoc{-1}, fogMaxDistLoc{-1};
    int fogHeightFadeLoc{-1}, fogZHeightLoc{-1}, fogDitherBlendLoc{-1}, fogColorLoc{-1};
    int fogNearLoc{-1}, fogFarLoc{-1}, depthTexLoc{-1};
    int camPosLoc{-1}, camFwdLoc{-1}, camRightLoc{-1}, camUpLoc{-1}, camFovLoc{-1};
    int fogNoiseScaleLoc{-1}, fogNoiseStrengthLoc{-1};
    int fogWindOffsetLoc{-1}, fogTimeLoc{-1};

    void CacheLocations(Shader s);
};