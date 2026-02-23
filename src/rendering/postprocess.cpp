#include "postprocess.h"
#include "rlgl.h"

void PostProcess::CacheLocations(Shader s) {
    s.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(s, "mvp");

    resolutionLoc       = GetShaderLocation(s, "resolution");
    barrelEnabledLoc    = GetShaderLocation(s, "barrelEnabled");
    barrelStrengthLoc   = GetShaderLocation(s, "barrelStrength");
    barrelZoomLoc       = GetShaderLocation(s, "barrelZoom");
    ditherEnabledLoc    = GetShaderLocation(s, "ditherEnabled");
    ditherStrengthLoc   = GetShaderLocation(s, "ditherStrength");
    ditherColorDepthLoc = GetShaderLocation(s, "ditherColorDepth");

    // Fog
    fogEnabledLoc    = GetShaderLocation(s, "fogEnabled");
    fogDensityLoc    = GetShaderLocation(s, "fogDensity");
    fogStartLoc      = GetShaderLocation(s, "fogStart");
    fogMaxDistLoc    = GetShaderLocation(s, "fogMaxDist");
    fogHeightFadeLoc = GetShaderLocation(s, "fogHeightFade");
    fogDitherBlendLoc= GetShaderLocation(s, "fogDitherBlend");
    fogColorLoc      = GetShaderLocation(s, "fogColor");
    fogNearLoc       = GetShaderLocation(s, "fogNear");
    fogFarLoc        = GetShaderLocation(s, "fogFar");
    depthTexLoc      = GetShaderLocation(s, "depthTexture");
}

void PostProcess::Init(int w, int h, const char* vsPath, const char* fsPath) {
    width = w;
    height = h;
    target = LoadRenderTexture(w, h);

    hotReload.Init(vsPath, fsPath, [this](Shader s) { CacheLocations(s); });
}

void PostProcess::Unload() {
    UnloadRenderTexture(target);
    hotReload.Unload();
}

void PostProcess::CheckReload() {
    hotReload.Poll();
}

void PostProcess::Resize(int w, int h) {
    if (w == width && h == height) return;
    UnloadRenderTexture(target);
    width = w;
    height = h;
    target = LoadRenderTexture(w, h);
}

void PostProcess::Begin() {
    BeginTextureMode(target);
    ClearBackground(RAYWHITE);
}

void PostProcess::End() {
    EndTextureMode();

    Shader s = hotReload.Get();

    float res[2] = { (float)width, (float)height };
    SetShaderValue(s, resolutionLoc, res, SHADER_UNIFORM_VEC2);

    // Barrel
    int be = barrel.enabled ? 1 : 0;
    SetShaderValue(s, barrelEnabledLoc,  &be,              SHADER_UNIFORM_INT);
    SetShaderValue(s, barrelStrengthLoc, &barrel.strength,  SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, barrelZoomLoc,     &barrel.zoom,      SHADER_UNIFORM_FLOAT);

    // Dither
    int de = dither.enabled ? 1 : 0;
    SetShaderValue(s, ditherEnabledLoc,    &de,                SHADER_UNIFORM_INT);
    SetShaderValue(s, ditherStrengthLoc,   &dither.strength,   SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, ditherColorDepthLoc, &dither.colorDepth, SHADER_UNIFORM_FLOAT);

    // Fog
    int fe = fog.enabled ? 1 : 0;
    SetShaderValue(s, fogEnabledLoc,     &fe,               SHADER_UNIFORM_INT);
    SetShaderValue(s, fogDensityLoc,     &fog.density,      SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, fogStartLoc,       &fog.startDist,    SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, fogMaxDistLoc,     &fog.maxDist,      SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, fogHeightFadeLoc,  &fog.heightFade,   SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, fogDitherBlendLoc, &fog.ditherBlend,  SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, fogNearLoc,        &fog.nearPlane,    SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, fogFarLoc,         &fog.farPlane,     SHADER_UNIFORM_FLOAT);

    float fc[3] = { fog.color.r / 255.0f, fog.color.g / 255.0f, fog.color.b / 255.0f };
    SetShaderValue(s, fogColorLoc, fc, SHADER_UNIFORM_VEC3);

    // Bind depth texture to sampler unit 1
    rlActiveTextureSlot(1);
    rlEnableTexture(target.depth.id);
    int depthSlot = 1;
    SetShaderValue(s, depthTexLoc, &depthSlot, SHADER_UNIFORM_INT);

    BeginDrawing();
        ClearBackground(BLACK);
        BeginShaderMode(s);
            DrawTextureRec(target.texture,
                           {0, 0, (float)width, -(float)height},
                           {0, 0}, WHITE);
        EndShaderMode();
    // EndDrawing() called by caller after HUD
}