#pragma once
#include "raylib.h"
#include <string>

// Returns the shader subdirectory for the current platform.
//   Desktop  → assets/shaders/gl330/   (OpenGL 3.3)
//   Web/RPi  → assets/shaders/es100/   (OpenGL ES 2.0)
inline const char* GetShaderPath() {
#if defined(__EMSCRIPTEN__) || defined(PLATFORM_RPI)
    return "assets/shaders/es100/";
#else
    return "assets/shaders/gl330/";
#endif
}

// Load a shader pair by name, automatically selecting the right GLSL version.
inline Shader LoadPlatformShader(const char* name) {
    std::string base = GetShaderPath();
    std::string vs = base + name + ".vs";
    std::string fs = base + name + ".fs";
    Shader s = LoadShader(vs.c_str(), fs.c_str());
    if (s.id == 0) {
        TraceLog(LOG_ERROR, "SHADER FAILED: %s", name);
    }
    return s;
}
// For shaders that only have a fragment stage (using raylib's default vertex shader)
inline Shader LoadPlatformFragShader(const char* name) {
    std::string base = GetShaderPath();
    std::string fs = base + name + ".fs";
    return LoadShader(nullptr, fs.c_str());
}