#include "postprocess.h"
#include "engine_config.h"
#include "shader_utils.h"
#include "world.h"
#include "rlgl.h"

static int NextPOT(int v) { int p = 1; while (p < v) p <<= 1; return p; }

static RenderTexture2D LoadRenderTextureCompat(int w, int h) {
#if defined(__EMSCRIPTEN__) || defined(PLATFORM_RPI)
    int pw = NextPOT(w);
    int ph = NextPOT(h);
    RenderTexture2D rt = LoadRenderTexture(pw, ph);
    SetTextureWrap(rt.texture, TEXTURE_WRAP_CLAMP);
#else
    RenderTexture2D rt = { 0 };
    rt.id = rlLoadFramebuffer();
    if (rt.id > 0) {
        rlEnableFramebuffer(rt.id);
        rt.texture.id     = rlLoadTexture(NULL, w, h, RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1);
        rt.texture.width  = w;
        rt.texture.height = h;
        rt.texture.format = RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
        rt.texture.mipmaps = 1;
        rt.depth.id     = rlLoadTextureDepth(w, h, true);
        rt.depth.width  = w;
        rt.depth.height = h;
        rlFramebufferAttach(rt.id, rt.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
        rlFramebufferAttach(rt.id, rt.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
        rlDisableFramebuffer();
    }
#endif
    return rt;
}

void PostProcess::CacheLocations(Shader s) {
    s.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(s, "mvp");

    resolutionLoc       = GetShaderLocation(s, "resolution");
    barrelEnabledLoc    = GetShaderLocation(s, "barrelEnabled");
    barrelStrengthLoc   = GetShaderLocation(s, "barrelStrength");
    barrelZoomLoc       = GetShaderLocation(s, "barrelZoom");
    ditherEnabledLoc    = GetShaderLocation(s, "ditherEnabled");
    ditherStrengthLoc   = GetShaderLocation(s, "ditherStrength");
    ditherColorDepthLoc = GetShaderLocation(s, "ditherColorDepth");

    fogEnabledLoc    = GetShaderLocation(s, "fogEnabled");
    fogDensityLoc    = GetShaderLocation(s, "fogDensity");
    fogStartLoc      = GetShaderLocation(s, "fogStart");
    fogMaxDistLoc    = GetShaderLocation(s, "fogMaxDist");
    fogHeightFadeLoc = GetShaderLocation(s, "fogHeightFade");
    fogZHeightLoc    = GetShaderLocation(s, "fogZHeight");
    fogDitherBlendLoc= GetShaderLocation(s, "fogDitherBlend");
    fogColorLoc      = GetShaderLocation(s, "fogColor");
    fogNearLoc       = GetShaderLocation(s, "fogNear");
    fogFarLoc        = GetShaderLocation(s, "fogFar");
    depthTexLoc      = GetShaderLocation(s, "depthTexture");
    camPosLoc        = GetShaderLocation(s, "camPos");
    camFwdLoc        = GetShaderLocation(s, "camFwd");
    camRightLoc      = GetShaderLocation(s, "camRight");
    camUpLoc         = GetShaderLocation(s, "camUp");
    camFovLoc        = GetShaderLocation(s, "camFov");
    fogNoiseScaleLoc    = GetShaderLocation(s, "fogNoiseScale");
    fogNoiseStrengthLoc = GetShaderLocation(s, "fogNoiseStrength");
    fogWindOffsetLoc    = GetShaderLocation(s, "fogWindOffset");
    fogTimeLoc          = GetShaderLocation(s, "fogTime");
}

void PostProcess::Init(int w, int h, int sw, int sh) {
    std::string vs = std::string(GetShaderPath()) + "postprocess.vs";
    std::string fs = std::string(GetShaderPath()) + "postprocess.fs";
    Init(w, h, sw, sh, vs.c_str(), fs.c_str());
}

void PostProcess::Init(int w, int h, int sw, int sh, const char* vsPath, const char* fsPath) {
    width = w;
    height = h;
    screenW = sw;
    screenH = sh;
    target = LoadRenderTextureCompat(w, h);
    SetTextureFilter(target.texture,
        EngineConfig::PIXEL_PERFECT ? TEXTURE_FILTER_POINT : TEXTURE_FILTER_BILINEAR);
    hotReload.Init(vsPath, fsPath, [this](Shader s) { CacheLocations(s); });
}

void PostProcess::Unload() {
    UnloadRenderTexture(target);
    hotReload.Unload();
}

void PostProcess::CheckReload() { hotReload.Poll(); }

void PostProcess::Resize(int w, int h) {
    if (w == width && h == height) return;
    UnloadRenderTexture(target);
    width = w;
    height = h;
    target = LoadRenderTextureCompat(w, h);
}

void PostProcess::Begin() {
    #if defined(__EMSCRIPTEN__) || defined(PLATFORM_RPI)
        BeginDrawing();
        ClearBackground(RAYWHITE);
    #else
        BeginTextureMode(target);
        ClearBackground(RAYWHITE);
    #endif
}

void PostProcess::End() {
    #if defined(__EMSCRIPTEN__) || defined(PLATFORM_RPI)
        return;  // caller draws HUD + EndDrawing
    #else
        EndTextureMode();

    Shader s = hotReload.Get();

    // Resolution uniform = logical render size, not FBO size
    float res[2] = { (float)width, (float)height };
    SetShaderValue(s, resolutionLoc, res, SHADER_UNIFORM_VEC2);

    // Barrel
    int be = barrel.enabled ? 1 : 0;
    SetShaderValue(s, barrelEnabledLoc,  &be,             SHADER_UNIFORM_INT);
    SetShaderValue(s, barrelStrengthLoc, &barrel.strength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, barrelZoomLoc,     &barrel.zoom,     SHADER_UNIFORM_FLOAT);

    // Dither
    int de = dither.enabled ? 1 : 0;
    SetShaderValue(s, ditherEnabledLoc,    &de,                SHADER_UNIFORM_INT);
    SetShaderValue(s, ditherStrengthLoc,   &dither.strength,   SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, ditherColorDepthLoc, &dither.colorDepth, SHADER_UNIFORM_FLOAT);

    // Fog
    int fe = World::fogEnabled ? 1 : 0;
    SetShaderValue(s, fogEnabledLoc,     &fe,                    SHADER_UNIFORM_INT);
    SetShaderValue(s, fogDensityLoc,     &World::fogDensity,     SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, fogStartLoc,       &World::fogStart,       SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, fogMaxDistLoc,     &World::fogMaxDist,     SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, fogHeightFadeLoc,  &World::fogHeightFade,  SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, fogZHeightLoc,     &World::fogZHeight,     SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, fogDitherBlendLoc, &World::fogDitherBlend, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, fogNearLoc,        &World::fogNear,        SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, fogFarLoc,         &World::fogFar,         SHADER_UNIFORM_FLOAT);

    // Camera vectors
    SetShaderValue(s, camPosLoc,   &World::cameraPos,   SHADER_UNIFORM_VEC3);
    SetShaderValue(s, camFwdLoc,   &World::cameraFwd,   SHADER_UNIFORM_VEC3);
    SetShaderValue(s, camRightLoc, &World::cameraRight,  SHADER_UNIFORM_VEC3);
    SetShaderValue(s, camUpLoc,    &World::cameraUp,     SHADER_UNIFORM_VEC3);
    float fovRad = World::cameraFov * DEG2RAD;
    SetShaderValue(s, camFovLoc,   &fovRad,              SHADER_UNIFORM_FLOAT);

    // Fog noise
    SetShaderValue(s, fogNoiseScaleLoc,    &World::fogNoiseScale,    SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, fogNoiseStrengthLoc, &World::fogNoiseStrength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, fogTimeLoc,          &World::worldTime,        SHADER_UNIFORM_FLOAT);

    float wLen = sqrtf(World::fogWindDir.x * World::fogWindDir.x +
                       World::fogWindDir.y * World::fogWindDir.y);
    float wNorm = (wLen > 0.001f) ? 1.0f / wLen : 0.0f;
    float windOff[2] = {
        World::fogWindDir.x * wNorm * World::fogWindSpeed * World::worldTime,
        World::fogWindDir.y * wNorm * World::fogWindSpeed * World::worldTime
    };
    SetShaderValue(s, fogWindOffsetLoc, windOff, SHADER_UNIFORM_VEC2);

    float fc[3] = {
        World::fogColor.r / 255.0f,
        World::fogColor.g / 255.0f,
        World::fogColor.b / 255.0f
    };
    SetShaderValue(s, fogColorLoc, fc, SHADER_UNIFORM_VEC3);

    // Depth texture — only on desktop (WebGL1 can't sample depth renderbuffers)
#if !defined(__EMSCRIPTEN__) && !defined(PLATFORM_RPI)
    rlActiveTextureSlot(1);
    rlEnableTexture(target.depth.id);
    int depthSlot = 1;
    SetShaderValue(s, depthTexLoc, &depthSlot, SHADER_UNIFORM_INT);
#endif

    BeginDrawing();
        ClearBackground(BLACK);
        BeginShaderMode(s);
            // Source rect = logical render size (sub-rect of possibly larger POT texture)
            DrawTexturePro(target.texture,
                           {0, 0, (float)width, -(float)height},
                           {0, 0, (float)screenW, (float)screenH},
                           {0, 0}, 0.0f, WHITE);
        EndShaderMode();
    // EndDrawing() called by caller after HUD
    #endif
}