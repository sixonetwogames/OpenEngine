#pragma once
#include "raylib.h"
#include "hot_reload_shader.h"
#include <vector>

struct ShadowCaster {
    Vector3 position;
    Vector3 size;
};

struct ShadowSettings {
    Vector3 color       = {0.0f, 0.0f, 0.0f};
    float   opacity     = 0.45f;
    float   sunOffset   = 0.4f;
    float   sizeScale   = 3.0f;
    float   maxStretch  = 3.0f;   // max elongation at dawn/dusk (1.0 = no stretch)
    bool    enabled     = true;
};

class ShadowSystem {
public:
    void Init(const char* vsPath = "assets/shaders/shadow.vs",
              const char* fsPath = "assets/shaders/shadow.fs");
    void Unload();
    void CheckReload();

    void SetSunDir(Vector3 dir) { sunDir = dir; }
    ShadowSettings& GetSettings() { return settings; }

    void Draw(const std::vector<ShadowCaster>& casters) const;

private:
    Vector3        sunDir{0, -1, 0};
    ShadowSettings settings;

    HotReloadShader hotReload;
    int    opacityLoc{-1};
    int    colorLoc{-1};
    Model  quadModel{};

    void CacheLocations(Shader s);
};