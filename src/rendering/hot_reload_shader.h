#pragma once
#include "raylib.h"
#include "shader_utils.h"
#include <string>
#include <functional>

class HotReloadShader {
public:
    using ReloadCallback = std::function<void(Shader)>;

    // Platform-aware: auto-selects gl330 or es100 by name
    void Init(const char* name, ReloadCallback onReload) {
        std::string base = GetShaderPath();
        Init((base + name + ".vs").c_str(),
             (base + name + ".fs").c_str(),
             onReload);
    }

    // Explicit paths
    void Init(const char* vsPath, const char* fsPath, ReloadCallback onReload) {
        vs = vsPath;
        fs = fsPath;
        callback = onReload;
        shader = LoadShader(vsPath, fsPath);
        vsModTime = GetFileModTime(vsPath);
        fsModTime = GetFileModTime(fsPath);
        callback(shader);
    }

    /// Call once per frame. Returns true if shader was reloaded.
    bool Poll() {
        long vsNow = GetFileModTime(vs.c_str());
        long fsNow = GetFileModTime(fs.c_str());

        if (vsNow != vsModTime || fsNow != fsModTime) {
            vsModTime = vsNow;
            fsModTime = fsNow;

            Shader next = LoadShader(vs.c_str(), fs.c_str());
            if (next.id > 0) {
                UnloadShader(shader);
                shader = next;
                callback(shader);
                TraceLog(LOG_INFO, "HOTRELOAD: %s / %s", vs.c_str(), fs.c_str());
                return true;
            }
        }
        return false;
    }

    void Unload() { UnloadShader(shader); }

    Shader Get() const { return shader; }

private:
    Shader shader{};
    std::string vs, fs;
    long vsModTime = 0, fsModTime = 0;
    ReloadCallback callback;
};