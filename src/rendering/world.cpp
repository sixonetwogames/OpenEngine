#include "world.h"
#include "rlgl.h"
#include <cmath>

#include "shader_utils.h"
// ---------------------------------------------------------------------------
// Internal state (file-scope, not exposed in header)
// ---------------------------------------------------------------------------
namespace {
    Model  skyModel  = {};
    Shader skyShader = {};

    int timeLoc         = -1;
    int viewPosLoc      = -1;
    int sunDirLoc       = -1;
    int sunColorLoc     = -1;
    int sunIntensityLoc = -1;
    int skyZenithLoc    = -1;
    int skyHorizonLoc   = -1;
    int skyGroundLoc    = -1;
    int cloudOpacityLoc = -1;
    int fogColorLoc_sky   = -1;
    int fogAmountLoc_sky  = -1;
    int fogHeightLoc_sky  = -1;
    int fogNoiseScaleLoc_sky    = -1;
    int fogNoiseStrengthLoc_sky = -1;
    int fogWindOffsetLoc_sky    = -1;

    SkyPreset presets[5] = {};

    Vector3 V3Lerp(Vector3 a, Vector3 b, float t) {
        return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
    }
    float Clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
}

// ---------------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------------
static void InitDefaultPresets() {
    presets[0] = { // Night
        .zenith = {0.07f, 0.09f, 0.3f}, .horizon = {0.21f, 0.24f, 0.62f},
        .ground = {0.01f, 0.01f, 0.03f}, .sunColor = {0.3f, 0.3f, 0.5f},
        .sunIntensity = 0.0f, .cloudOpacity = 0.15f,
    };
    presets[1] = { // Dawn
        .zenith = {0.15f, 0.15f, 0.35f}, .horizon = {0.85f, 0.45f, 0.25f},
        .ground = {0.12f, 0.10f, 0.08f}, .sunColor = {1.0f, 0.6f, 0.3f},
        .sunIntensity = 0.8f, .cloudOpacity = 0.5f,
    };
    presets[2] = { // Day
        .zenith = {0.15f, 0.25f, 0.55f}, .horizon = {0.60f, 0.70f, 0.85f},
        .ground = {0.20f, 0.25f, 0.20f}, .sunColor = {1.0f, 0.95f, 0.85f},
        .sunIntensity = 1.0f, .cloudOpacity = 0.6f,
    };
    presets[3] = { // Sunset
        .zenith = {0.12f, 0.10f, 0.30f}, .horizon = {0.90f, 0.35f, 0.15f},
        .ground = {0.10f, 0.08f, 0.06f}, .sunColor = {1.0f, 0.5f, 0.2f},
        .sunIntensity = 0.7f, .cloudOpacity = 0.55f,
    };
    presets[4] = { // Night
        .zenith = {0.07f, 0.09f, 0.3f}, .horizon = {0.21f, 0.24f, 0.62f},
        .ground = {0.01f, 0.01f, 0.03f}, .sunColor = {0.3f, 0.3f, 0.5f},
        .sunIntensity = 0.0f, .cloudOpacity = 0.15f,
    };
}

// ---------------------------------------------------------------------------
// Remap day progress — compress dawn/dusk transitions
// ---------------------------------------------------------------------------
static float RemapDayProgress(float t) {
    float avgWidth = (World::dawnWidth + World::duskWidth) * 0.5f;
    float amp = (0.25f - Clamp01(avgWidth)) * 0.4f;
    float remapped = t - amp * sinf(4.0f * PI * t);
    if (remapped < 0.0f) remapped += 1.0f;
    if (remapped >= 1.0f) remapped -= 1.0f;
    return remapped;
}

static SkyPreset BlendPresets(float t) {
    float scaled = t * 4.0f;
    int idx0 = (int)scaled % 4;
    int idx1 = (idx0 + 1) % 4;
    float frac = scaled - (int)scaled;
    float s = frac * frac * (3.0f - 2.0f * frac);

    const SkyPreset& a = presets[idx0];
    const SkyPreset& b = presets[idx1];

    return {
        .zenith       = V3Lerp(a.zenith, b.zenith, s),
        .horizon      = V3Lerp(a.horizon, b.horizon, s),
        .ground       = V3Lerp(a.ground, b.ground, s),
        .sunColor     = V3Lerp(a.sunColor, b.sunColor, s),
        .sunIntensity = a.sunIntensity + (b.sunIntensity - a.sunIntensity) * s,
        .cloudOpacity = a.cloudOpacity + (b.cloudOpacity - a.cloudOpacity) * s,
    };
}

// ---------------------------------------------------------------------------
// Compute sun/moon lighting + skylight
// ---------------------------------------------------------------------------
static void ComputeLighting(float t) {
    float sunAngle = (t - 0.25f) * PI * 2.0f;
    float sunElev = sinf(sunAngle);

    Vector3 sunDir = Vector3Normalize({cosf(sunAngle), -sinf(sunAngle), -0.3f});
    Vector3 moonDir = {-sunDir.x, -sunDir.y, -sunDir.z};

    float sunFade = Clamp01((sunElev - World::sunFadeEnd) /
                            (World::sunFadeStart - World::sunFadeEnd + 0.001f));
    sunFade *= sunFade;

    float moonFade = Clamp01(-sunElev / 0.15f);
    moonFade *= moonFade;

    // Sun elevation drives intensity lerp (0 at horizon → 1 at zenith)
    float dayFactor = Clamp01(sunElev / 0.5f);

    if (sunElev > 0.0f) {
        World::lightDir       = sunDir;
        World::lightColor     = World::currentSky.sunColor;
        World::lightIntensity = World::currentSky.sunIntensity * sunFade *
                                (World::nightSunIntensity + (World::sunPeakIntensity - World::nightSunIntensity) * dayFactor);
    } else {
        World::lightDir       = moonDir;
        World::lightColor     = World::moonColor;
        World::lightIntensity = World::moonIntensity * moonFade * World::nightSunIntensity;
    }

    float h = World::skylightHeight;
    World::skylightColor     = V3Lerp(World::currentSky.horizon, World::currentSky.zenith, h);
    World::skylightIntensity = World::nightSkylightIntensity +
                               (World::skylightPeakIntensity - World::nightSkylightIntensity) * dayFactor;
}

// ---------------------------------------------------------------------------
// Fog color: base tinted by skylight
// ---------------------------------------------------------------------------
static void ComputeFogColor() {
    const SkyPreset& sky = World::currentSky;
    const Vector3& skylight = World::skylightColor;

    // Start from stable base
    Vector3 base = {
        World::fogBaseColor.r / 255.0f,
        World::fogBaseColor.g / 255.0f,
        World::fogBaseColor.b / 255.0f
    };

    // Blend in sky ground hemisphere color
    base = V3Lerp(base, sky.ground, World::fogGroundBlend);

    // Tint by skylight
    base.x += skylight.x * World::fogSkyTint;
    base.y += skylight.y * World::fogSkyTint;
    base.z += skylight.z * World::fogSkyTint;

    // Day/night brightness: darker at night, lighter during day
    float dayBright = 0.3f + 0.7f * sky.sunIntensity;
    base.x *= dayBright;
    base.y *= dayBright;
    base.z *= dayBright;

    World::fogColor = {
        (unsigned char)(fminf(base.x * 255.0f, 255.0f)),
        (unsigned char)(fminf(base.y * 255.0f, 255.0f)),
        (unsigned char)(fminf(base.z * 255.0f, 255.0f)),
        255
    };
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void World::Init() {
    InitDefaultPresets();
    dayProgress = startDayProgress;

    Mesh mesh = GenMeshSphere(SKY_SPHERE_RADIUS, SKY_SPHERE_RINGS, SKY_SPHERE_SLICES);
    skyModel  = LoadModelFromMesh(mesh);
    skyShader = LoadPlatformShader("sky");
    skyModel.materials[0].shader = skyShader;

    timeLoc         = GetShaderLocation(skyShader, "time");
    viewPosLoc      = GetShaderLocation(skyShader, "viewPos");
    sunDirLoc       = GetShaderLocation(skyShader, "sunDir");
    sunColorLoc     = GetShaderLocation(skyShader, "sunColor");
    sunIntensityLoc = GetShaderLocation(skyShader, "sunIntensity");
    skyZenithLoc    = GetShaderLocation(skyShader, "skyZenith");
    skyHorizonLoc   = GetShaderLocation(skyShader, "skyHorizon");
    skyGroundLoc    = GetShaderLocation(skyShader, "skyGround");
    cloudOpacityLoc = GetShaderLocation(skyShader, "cloudOpacity");
    fogColorLoc_sky  = GetShaderLocation(skyShader, "fogColor");
    fogAmountLoc_sky = GetShaderLocation(skyShader, "fogAmount");
    fogHeightLoc_sky = GetShaderLocation(skyShader, "fogHeight");
    fogNoiseScaleLoc_sky    = GetShaderLocation(skyShader, "fogNoiseScale");
    fogNoiseStrengthLoc_sky = GetShaderLocation(skyShader, "fogNoiseStrength");
    fogWindOffsetLoc_sky    = GetShaderLocation(skyShader, "fogWindOffset");
}

void World::Unload() {
    UnloadShader(skyShader);
    UnloadModel(skyModel);
}

void World::Update(Camera camera) {
    float dt = GetFrameTime();
    worldTime += dt * timeScale;

    dayProgress += (dt * timeScale) / dayLengthSec;
    if (dayProgress >= 1.0f) dayProgress -= 1.0f;
    if (dayProgress < 0.0f)  dayProgress += 1.0f;

    float remapped = RemapDayProgress(dayProgress);
    currentSky = BlendPresets(remapped);
    ComputeLighting(dayProgress);
    ComputeFogColor();

    // Sky shader uniforms
    float sunAngle = (dayProgress - 0.25f) * PI * 2.0f;
    Vector3 skyShaderSunDir = Vector3Normalize({cosf(sunAngle), -sinf(sunAngle), -0.3});

    SetShaderValue(skyShader, timeLoc,         &worldTime,               SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, sunDirLoc,       &skyShaderSunDir,         SHADER_UNIFORM_VEC3);
    SetShaderValue(skyShader, sunColorLoc,     &currentSky.sunColor,     SHADER_UNIFORM_VEC3);
    SetShaderValue(skyShader, sunIntensityLoc, &currentSky.sunIntensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, skyZenithLoc,    &currentSky.zenith,       SHADER_UNIFORM_VEC3);
    SetShaderValue(skyShader, skyHorizonLoc,   &currentSky.horizon,      SHADER_UNIFORM_VEC3);
    SetShaderValue(skyShader, skyGroundLoc,    &currentSky.ground,       SHADER_UNIFORM_VEC3);
    SetShaderValue(skyShader, cloudOpacityLoc, &currentSky.cloudOpacity, SHADER_UNIFORM_FLOAT);

    Vector3 pos = camera.position;
    SetShaderValue(skyShader, viewPosLoc, &pos, SHADER_UNIFORM_VEC3);

    // Store camera state for post-process world reconstruction
    cameraPos   = pos;
    cameraFwd   = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    cameraRight = Vector3Normalize(Vector3CrossProduct(cameraFwd, camera.up));
    cameraUp    = Vector3CrossProduct(cameraRight, cameraFwd);
    cameraFov   = camera.fovy;

    // Fog → sky shader
    float fogCol[3] = { fogColor.r/255.0f, fogColor.g/255.0f, fogColor.b/255.0f };
    SetShaderValue(skyShader, fogHeightLoc_sky, &fogHeight,    SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, fogColorLoc_sky,  fogCol,        SHADER_UNIFORM_VEC3);
    SetShaderValue(skyShader, fogAmountLoc_sky, &fogSkyAmount, SHADER_UNIFORM_FLOAT);

    // Fog noise → sky shader
    SetShaderValue(skyShader, fogNoiseScaleLoc_sky,    &fogNoiseScale,    SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, fogNoiseStrengthLoc_sky, &fogNoiseStrength, SHADER_UNIFORM_FLOAT);
    float wLen = sqrtf(fogWindDir.x * fogWindDir.x + fogWindDir.y * fogWindDir.y);
    float wN = (wLen > 0.001f) ? 1.0f / wLen : 0.0f;
    float windOff[2] = {
        fogWindDir.x * wN * fogWindSpeed * worldTime,
        fogWindDir.y * wN * fogWindSpeed * worldTime
    };
    SetShaderValue(skyShader, fogWindOffsetLoc_sky, windOff, SHADER_UNIFORM_VEC2);
}

void World::DrawSky(Camera camera) {
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
        DrawModel(skyModel, camera.position, 1.0f, WHITE);
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
}

void World::SetDayProgress(float t) {
    dayProgress = t;
    if (dayProgress >= 1.0f) dayProgress -= 1.0f;
    if (dayProgress < 0.0f)  dayProgress += 1.0f;
}

void World::SetPreset(int index, const SkyPreset& preset) {
    if (index >= 0 && index < 4) presets[index] = preset;
}