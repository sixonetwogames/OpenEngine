#pragma once
#include "raylib.h"
#include "material_instance.h"
#include <string>
#include <unordered_map>
#include <functional>

// ─── ResourceManager ────────────────────────────────────────────────────────
// Centralized load-once store for shaders, textures, models, and materials.
// Deduplicates by string key. Call Shutdown() once at exit.
//
// Usage:
//   auto& rm = ResourceManager::Get();
//   rm.LoadTexture("grass_d", "assets/textures/grass_D.png");
//   Texture2D tex = rm.GetTexture("grass_d");

class ResourceManager {
public:
    // Singleton — one instance for the whole engine
    static ResourceManager& Get() {
        static ResourceManager instance;
        return instance;
    }

    // ── Textures ─────────────────────────────────────────────────────────

    Texture2D LoadTex(const std::string& key, const char* path) {
        auto it = textures.find(key);
        if (it != textures.end()) return it->second;
        Texture2D tex = ::LoadTexture(path);
        if (tex.id == 0) TraceLog(LOG_WARNING, "ResourceManager: Failed to load texture '%s'", path);
        textures[key] = tex;
        return tex;
    }

    Texture2D GetTexture(const std::string& key) const {
        auto it = textures.find(key);
        if (it != textures.end()) return it->second;
        TraceLog(LOG_WARNING, "ResourceManager: Texture '%s' not found", key.c_str());
        return {};
    }

    bool HasTexture(const std::string& key) const { return textures.count(key) > 0; }

    // ── Shaders ──────────────────────────────────────────────────────────

    Shader LoadShad(const std::string& key, const char* vsPath, const char* fsPath) {
        auto it = shaders.find(key);
        if (it != shaders.end()) return it->second.shader;

        Shader s = ::LoadShader(vsPath, fsPath);
        if (s.id == 0) TraceLog(LOG_WARNING, "ResourceManager: Failed to load shader '%s'", key.c_str());
        shaders[key] = { s, vsPath ? vsPath : "", fsPath ? fsPath : "" };
        return s;
    }

    Shader GetShader(const std::string& key) const {
        auto it = shaders.find(key);
        if (it != shaders.end()) return it->second.shader;
        TraceLog(LOG_WARNING, "ResourceManager: Shader '%s' not found", key.c_str());
        return {};
    }

    // Reload a shader from its original paths (hot-reload)
    bool ReloadShader(const std::string& key) {
        auto it = shaders.find(key);
        if (it == shaders.end()) return false;
        auto& entry = it->second;
        Shader reloaded = ::LoadShader(
            entry.vsPath.empty() ? nullptr : entry.vsPath.c_str(),
            entry.fsPath.empty() ? nullptr : entry.fsPath.c_str()
        );
        if (reloaded.id == 0) return false;
        ::UnloadShader(entry.shader);
        entry.shader = reloaded;
        return true;
    }

    // ── Models ───────────────────────────────────────────────────────────

    Model LoadMdl(const std::string& key, const char* path) {
        auto it = models.find(key);
        if (it != models.end()) return it->second;
        Model m = ::LoadModel(path);
        models[key] = m;
        return m;
    }

    // Store a procedurally-generated model
    void StoreMdl(const std::string& key, Model m) {
        auto it = models.find(key);
        if (it != models.end()) {
            ::UnloadModel(it->second);
        }
        models[key] = m;
    }

    Model GetModel(const std::string& key) const {
        auto it = models.find(key);
        if (it != models.end()) return it->second;
        TraceLog(LOG_WARNING, "ResourceManager: Model '%s' not found", key.c_str());
        return {};
    }

    bool HasModel(const std::string& key) const { return models.count(key) > 0; }

    // ── Materials ────────────────────────────────────────────────────────

    MaterialInstance& CreateMaterial(const std::string& key) {
        return materials[key]; // default-constructs if new
    }

    MaterialInstance& GetMaterial(const std::string& key) {
        auto it = materials.find(key);
        if (it != materials.end()) return it->second;
        TraceLog(LOG_WARNING, "ResourceManager: Material '%s' not found, creating empty", key.c_str());
        return materials[key];
    }

    const MaterialInstance& GetMaterial(const std::string& key) const {
        static MaterialInstance empty;
        auto it = materials.find(key);
        return it != materials.end() ? it->second : empty;
    }

    bool HasMaterial(const std::string& key) const { return materials.count(key) > 0; }

    // ── Reload all shaders (press F5 in debug) ──────────────────────────

    int ReloadAllShaders() {
        int count = 0;
        for (auto& [key, _] : shaders) {
            if (ReloadShader(key)) count++;
        }
        return count;
    }

    // ── Cleanup ──────────────────────────────────────────────────────────

    void Shutdown() {
        for (auto& [_, mat] : materials) mat.Unload();
        for (auto& [_, entry] : shaders) ::UnloadShader(entry.shader);
        for (auto& [_, tex] : textures)  ::UnloadTexture(tex);
        for (auto& [_, mdl] : models)    ::UnloadModel(mdl);
        materials.clear();
        shaders.clear();
        textures.clear();
        models.clear();
    }

private:
    ResourceManager() = default;
    ~ResourceManager() = default;
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    struct ShaderEntry {
        Shader      shader;
        std::string vsPath;
        std::string fsPath;
    };

    std::unordered_map<std::string, Texture2D>    textures;
    std::unordered_map<std::string, ShaderEntry>   shaders;
    std::unordered_map<std::string, Model>         models;
    std::unordered_map<std::string, MaterialInstance>      materials;
};