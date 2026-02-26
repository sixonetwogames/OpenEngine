#pragma once
#include "raylib.h"
#include "hot_reload_shader.h"
#include "world.h"
#include <vector>

struct ShadowCaster {
    Vector3 position;
    Vector3 size;
    float   sunOffsetOverride = -1.0f;   // <0 = use World::shadowSunOffset
    bool    enabled           =  true;    // quick toggle without removing
};

class ShadowSystem {
public:
    void Init();
    void Init(const char* vsPath, const char* fsPath);
    void Unload();
    void CheckReload();

    void SetSunDir(Vector3 dir) { sunDir = dir; }
    void Draw(const std::vector<ShadowCaster>& casters) const;

    // Convenience: append billboard-generated casters onto an existing list
    static void AppendCasters(std::vector<ShadowCaster>& out,
                              const ShadowCaster* src, size_t count) {
        out.insert(out.end(), src, src + count);
    }

private:
    Vector3 sunDir{0, -1, 0};

    HotReloadShader hotReload;
    int    opacityLoc{-1};
    int    colorLoc{-1};
    Model  quadModel{};

    void CacheLocations(Shader s);
};