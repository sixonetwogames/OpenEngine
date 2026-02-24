#pragma once
#include "raylib.h"
#include "hot_reload_shader.h"
#include "world.h"
#include <vector>

struct ShadowCaster {
    Vector3 position;
    Vector3 size;
};

class ShadowSystem {
public:
    void Init(const char* vsPath = World::SHADOW_VS_PATH,
              const char* fsPath = World::SHADOW_FS_PATH);
    void Unload();
    void CheckReload();

    void SetSunDir(Vector3 dir) { sunDir = dir; }
    void Draw(const std::vector<ShadowCaster>& casters) const;

private:
    Vector3 sunDir{0, -1, 0};

    HotReloadShader hotReload;
    int    opacityLoc{-1};
    int    colorLoc{-1};
    Model  quadModel{};

    void CacheLocations(Shader s);
};