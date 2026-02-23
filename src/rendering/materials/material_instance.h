#pragma once
#include "raylib.h"
#include "raymath.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstring>

// ─── Material parameters ────────────────────────────────────────────────────
// Matches uniforms in pbr.fs exactly.
struct MaterialParams {
    Color   albedo          = WHITE;       // → albedoColor (vec3)
    float   metallic        = 0.0f;        // → metallic
    float   roughness       = 0.5f;        // → roughness
    float   ambient         = 0.1f;        // → ambientStrength
    bool    useNormalMap     = true;        // → useNormalMap (int)
    float   normalStrength   = 1.0f;       // → normalStrength
    float   brightness       = 1.0f;       // → brightness
    float   contrast         = 1.0f;       // → contrast
    bool    useWorldUVs      = true;       // → useWorldUVs (int)
    float   tiling           = 1.0f;       // → tiling
    bool    useRoughnessMap  = false;      // → useRoughnessMap (int)
};

// ─── Texture slot identifiers ───────────────────────────────────────────────
enum class TexSlot : uint8_t {
    Albedo = 0,
    Normal,
    Roughness,
    Metallic,
    Emissive,
    AO,
    Count
};

// ─── MaterialInstance ───────────────────────────────────────────────────────
// One class for all material types. Shader + textures + params = a material.

class MaterialInstance {
    Shader  shader      = {};
    bool    ownsShader  = false;
    std::string shaderKey;
    std::string storedVsPath;
    std::string storedFsPath;

    Texture2D textures[static_cast<size_t>(TexSlot::Count)] = {};
    bool      hasTexture[static_cast<size_t>(TexSlot::Count)] = {};

    // Cached uniform locations — names match pbr.fs
    int locAlbedoColor     = -1;
    int locMetallic        = -1;
    int locRoughness       = -1;
    int locAmbientStrength = -1;
    int locAmbientColor    = -1;
    int locUseNormalMap    = -1;
    int locNormalStrength  = -1;
    int locBrightness      = -1;
    int locContrast        = -1;
    int locUseWorldUVs     = -1;
    int locTiling          = -1;
    int locUseRoughnessMap = -1;
    int locViewPos         = -1;
    int locLightDir        = -1;
    int locLightColor      = -1;
    int locFogNear         = -1;
    int locFogFar          = -1;

    std::unordered_map<std::string, int> customLocs;

    void CacheLocations() {
        locAlbedoColor     = GetShaderLocation(shader, "albedoColor");
        locMetallic        = GetShaderLocation(shader, "metallic");
        locRoughness       = GetShaderLocation(shader, "roughness");
        locAmbientStrength = GetShaderLocation(shader, "ambientStrength");
        locAmbientColor    = GetShaderLocation(shader, "ambientColor");
        locUseNormalMap    = GetShaderLocation(shader, "useNormalMap");
        locNormalStrength  = GetShaderLocation(shader, "normalStrength");
        locBrightness      = GetShaderLocation(shader, "brightness");
        locContrast        = GetShaderLocation(shader, "contrast");
        locUseWorldUVs     = GetShaderLocation(shader, "useWorldUVs");
        locTiling          = GetShaderLocation(shader, "tiling");
        locUseRoughnessMap = GetShaderLocation(shader, "useRoughnessMap");
        locViewPos         = GetShaderLocation(shader, "viewPos");
        locLightDir        = GetShaderLocation(shader, "lightDir");
        locLightColor      = GetShaderLocation(shader, "lightColor");
        locFogNear         = GetShaderLocation(shader, "fogNear");
        locFogFar          = GetShaderLocation(shader, "fogFar");

        TraceLog(LOG_INFO, "MAT LOCS: albedo=%d metal=%d rough=%d ambStr=%d ambCol=%d norm=%d light=%d lightCol=%d fog=%d/%d",
        locAlbedoColor, locMetallic, locRoughness, locAmbientStrength, locAmbientColor,
        locUseNormalMap, locLightDir, locLightColor, locFogNear, locFogFar);
    }

    static void SetLoc(Shader s, int loc, const void* val, int uniformType) {
        if (loc >= 0) SetShaderValue(s, loc, val, uniformType);
    }

public:
    // ── Setup ────────────────────────────────────────────────────────────

    void Init(Shader s, bool takeOwnership = false) {
        shader = s;
        ownsShader = takeOwnership;
        CacheLocations();
    }

    void Init(const char* vsPath, const char* fsPath) {
        storedVsPath = vsPath ? vsPath : "";
        storedFsPath = fsPath ? fsPath : "";
        shader = ::LoadShader(vsPath, fsPath);
        ownsShader = true;
        CacheLocations();
    }

    void SetTexture(TexSlot slot, Texture2D tex) {
        size_t i = static_cast<size_t>(slot);
        textures[i] = tex;
        hasTexture[i] = true;
    }

    // ── Apply to a raylib Model (sets shader + texture maps) ─────────────

    void Apply(Model& model) const {
        for (int i = 0; i < model.materialCount; i++) {
            model.materials[i].shader = shader;

            if (hasTexture[static_cast<size_t>(TexSlot::Albedo)])
                model.materials[i].maps[MATERIAL_MAP_ALBEDO].texture =
                    textures[static_cast<size_t>(TexSlot::Albedo)];

            if (hasTexture[static_cast<size_t>(TexSlot::Normal)])
                model.materials[i].maps[MATERIAL_MAP_NORMAL].texture =
                    textures[static_cast<size_t>(TexSlot::Normal)];

            if (hasTexture[static_cast<size_t>(TexSlot::Roughness)])
                model.materials[i].maps[MATERIAL_MAP_ROUGHNESS].texture =
                    textures[static_cast<size_t>(TexSlot::Roughness)];

            if (hasTexture[static_cast<size_t>(TexSlot::Metallic)])
                model.materials[i].maps[MATERIAL_MAP_METALNESS].texture =
                    textures[static_cast<size_t>(TexSlot::Metallic)];

            if (hasTexture[static_cast<size_t>(TexSlot::AO)])
                model.materials[i].maps[MATERIAL_MAP_OCCLUSION].texture =
                    textures[static_cast<size_t>(TexSlot::AO)];
        }
    }

    // ── Bind per-draw params (call before DrawModel) ─────────────────────

    void Bind(const MaterialParams& p) const {
        // albedoColor is vec3 in the shader
        float albedo[3] = { p.albedo.r/255.0f, p.albedo.g/255.0f, p.albedo.b/255.0f };
        int useNorm    = p.useNormalMap    ? 1 : 0;
        int useWorldUV = p.useWorldUVs     ? 1 : 0;
        int useRoughMap = p.useRoughnessMap ? 1 : 0;

        SetLoc(shader, locAlbedoColor,     albedo,              SHADER_UNIFORM_VEC3);
        SetLoc(shader, locMetallic,        &p.metallic,         SHADER_UNIFORM_FLOAT);
        SetLoc(shader, locRoughness,       &p.roughness,        SHADER_UNIFORM_FLOAT);
        SetLoc(shader, locAmbientStrength, &p.ambient,          SHADER_UNIFORM_FLOAT);
        SetLoc(shader, locUseNormalMap,    &useNorm,            SHADER_UNIFORM_INT);
        SetLoc(shader, locNormalStrength,  &p.normalStrength,   SHADER_UNIFORM_FLOAT);
        SetLoc(shader, locBrightness,      &p.brightness,       SHADER_UNIFORM_FLOAT);
        SetLoc(shader, locContrast,        &p.contrast,         SHADER_UNIFORM_FLOAT);
        SetLoc(shader, locUseWorldUVs,     &useWorldUV,         SHADER_UNIFORM_INT);
        SetLoc(shader, locTiling,          &p.tiling,           SHADER_UNIFORM_FLOAT);
        SetLoc(shader, locUseRoughnessMap, &useRoughMap,        SHADER_UNIFORM_INT);
    }

    // ── Scene-wide uniforms (call once per frame) ────────────────────────

    void SetCamera(Vector3 pos) const {
        SetLoc(shader, locViewPos, &pos, SHADER_UNIFORM_VEC3);
    }

    // lightIntensity doesn't exist as a separate uniform in your shader,
    // so we bake it into the color before uploading.
    void SetSun(Vector3 dir, Vector3 color, float intensity) const {
        Vector3 scaled = { color.x * intensity, color.y * intensity, color.z * intensity };
        SetLoc(shader, locLightDir,   &dir,    SHADER_UNIFORM_VEC3);
        SetLoc(shader, locLightColor, &scaled, SHADER_UNIFORM_VEC3);
    }

    // ambientColor in the shader — skylight maps to this.
    // Same approach: bake intensity into color.
    void SetSkylight(Vector3 color, float intensity) const {
        Vector3 scaled = { color.x * intensity, color.y * intensity, color.z * intensity };
        SetLoc(shader, locAmbientColor, &scaled, SHADER_UNIFORM_VEC3);
    }

    void SetFogPlanes(float near, float far) const {
        SetLoc(shader, locFogNear, &near, SHADER_UNIFORM_FLOAT);
        SetLoc(shader, locFogFar,  &far,  SHADER_UNIFORM_FLOAT);
    }

    // ── Custom uniforms ──────────────────────────────────────────────────

    void SetFloat(const char* name, float v) {
        int loc = GetOrCacheLoc(name);
        if (loc >= 0) SetShaderValue(shader, loc, &v, SHADER_UNIFORM_FLOAT);
    }

    void SetVec3(const char* name, Vector3 v) {
        int loc = GetOrCacheLoc(name);
        if (loc >= 0) SetShaderValue(shader, loc, &v, SHADER_UNIFORM_VEC3);
    }

    void SetInt(const char* name, int v) {
        int loc = GetOrCacheLoc(name);
        if (loc >= 0) SetShaderValue(shader, loc, &v, SHADER_UNIFORM_INT);
    }

    // ── Hot-reload support ───────────────────────────────────────────────
    // Only reloads on F5 keypress to avoid recompiling every frame.

    bool CheckReload() {
        if (!IsKeyPressed(KEY_F5)) return false;
        if (storedVsPath.empty() && storedFsPath.empty()) return false;
        return CheckReload(
            storedVsPath.empty() ? nullptr : storedVsPath.c_str(),
            storedFsPath.empty() ? nullptr : storedFsPath.c_str()
        );
    }

    bool CheckReload(const char* vsPath, const char* fsPath) {
        Shader reloaded = ::LoadShader(vsPath, fsPath);
        if (reloaded.id > 0) {
            if (ownsShader) UnloadShader(shader);
            shader = reloaded;
            ownsShader = true;
            CacheLocations();
            customLocs.clear();
            return true;
        }
        return false;
    }

    Shader GetShader() const { return shader; }

    void Unload() {
        if (ownsShader) UnloadShader(shader);
        shader = {};
    }

private:
    int GetOrCacheLoc(const char* name) {
        auto it = customLocs.find(name);
        if (it != customLocs.end()) return it->second;
        int loc = GetShaderLocation(shader, name);
        customLocs[name] = loc;
        return loc;
    }
};