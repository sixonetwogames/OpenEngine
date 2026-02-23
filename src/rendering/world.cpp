#include "world.h"
#include "rlgl.h"
#include <cmath>

static Vector3 V3Lerp(Vector3 a, Vector3 b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}

static float Clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

// ---------------------------------------------------------------------------
// Presets — same as before
// ---------------------------------------------------------------------------
void World::InitDefaultPresets() {
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
}

// ---------------------------------------------------------------------------
// Remap day progress so dawn/dusk transitions are shorter.
// Sine-based: flattens near stable presets (night/day), steepens transitions.
// ---------------------------------------------------------------------------
float World::RemapDayProgress(float t) const {
    // Amplitude controls compression — smaller dawnWidth = more compression
    float avgWidth = (dayCycle.dawnWidth + dayCycle.duskWidth) * 0.5f;
    float amp = (0.25f - Clamp01(avgWidth)) * 0.4f;  // 0 when width=0.25 (linear), ~0.08 when width=0.06

    // sin(4πt) has zeros at 0, 0.25, 0.5, 0.75, 1.0
    // Subtracting flattens the curve at those points (more time at stable presets)
    // and steepens between them (faster transitions = shorter dawn/dusk)
    float remapped = t - amp * sinf(4.0f * PI * t);

    // Keep in 0-1 range
    if (remapped < 0.0f) remapped += 1.0f;
    if (remapped >= 1.0f) remapped -= 1.0f;

    return remapped;
}

SkyPreset World::BlendPresets(float t) const {
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
void World::ComputeLighting(float t) {
    // Sun orbit: t=0.25 dawn (horizon east), t=0.5 noon, t=0.75 sunset
    float sunAngle = (t - 0.25f) * PI * 2.0f;
    float sunElev = sinf(sunAngle);   // +1 at noon, -1 at midnight

    Vector3 sunDir = Vector3Normalize({
        cosf(sunAngle),
        -sinf(sunAngle),
        -0.3f
    });

    // Moon is opposite the sun
    Vector3 moonDir = {-sunDir.x, -sunDir.y, -sunDir.z};

    // Sun intensity fades smoothly at horizon
    float sunFade = Clamp01((sunElev - dayCycle.sunFadeEnd) /
                            (dayCycle.sunFadeStart - dayCycle.sunFadeEnd + 0.001f));
    sunFade = sunFade * sunFade; // ease

    // Moon fades in when sun fades out
    float moonElev = -sunElev;  // opposite of sun
    float moonFade = Clamp01((moonElev - 0.0f) / 0.15f);
    moonFade = moonFade * moonFade;

    // Pick dominant light
    if (sunElev > 0.0f) {
        lighting.lightDir       = sunDir;
        lighting.lightColor     = currentSky.sunColor;
        lighting.lightIntensity = currentSky.sunIntensity * sunFade;
    } else {
        lighting.lightDir       = moonDir;
        lighting.lightColor     = dayCycle.moonColor;
        lighting.lightIntensity = dayCycle.moonIntensity * moonFade;
    }

    // Skylight: sample between horizon and zenith
    float h = dayCycle.skylightHeight;
    Vector3 skyColor = V3Lerp(currentSky.horizon, currentSky.zenith, h);
    lighting.skylightColor     = skyColor;
    lighting.skylightIntensity = dayCycle.skylightIntensity;
}

// ---------------------------------------------------------------------------
// Init / Unload
// ---------------------------------------------------------------------------
void World::Init() {
    InitDefaultPresets();

    Mesh mesh = GenMeshSphere(500.0f, 32, 32);
    skyModel  = LoadModelFromMesh(mesh);
    skyShader = LoadShader("assets/shaders/sky.vs", "assets/shaders/sky.fs");
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

    // In World::Init(), after other GetShaderLocation calls:
    fogColorLoc_sky  = GetShaderLocation(skyShader, "fogColor");
    fogAmountLoc_sky = GetShaderLocation(skyShader, "fogAmount");
    fogHeightLoc_sky = GetShaderLocation(skyShader, "fogHeight");
}

void World::Unload() {
    UnloadShader(skyShader);
    UnloadModel(skyModel);
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
void World::Update(Camera camera) {
    float dt = GetFrameTime();
    worldTime += dt * timeScale;

    dayProgress += (dt * timeScale) / dayLength;
    if (dayProgress >= 1.0f) dayProgress -= 1.0f;
    if (dayProgress < 0.0f)  dayProgress += 1.0f;

    // Remap for compressed dawn/dusk, then blend sky presets
    float remapped = RemapDayProgress(dayProgress);
    currentSky = BlendPresets(remapped);

    // Compute sun/moon/skylight from raw dayProgress (orbital, not remapped)
    ComputeLighting(dayProgress);

    // Push sky shader uniforms (uses remapped sky colors but raw sun dir for disk position)
    float sunAngle = (dayProgress - 0.25f) * PI * 2.0f;
    Vector3 skyShaderSunDir = Vector3Normalize({
        cosf(sunAngle), -sinf(sunAngle), -0.3f
    });

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

    float fogH = 0.4f;
    float fogCol[3] = { weather.fogColor.r/255.0f, weather.fogColor.g/255.0f, weather.fogColor.b/255.0f };
    SetShaderValue(skyShader, fogHeightLoc_sky, &fogH,                SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, fogColorLoc_sky,  fogCol,               SHADER_UNIFORM_VEC3);
    SetShaderValue(skyShader, fogAmountLoc_sky, &weather.fogDensity,  SHADER_UNIFORM_FLOAT);
}

void World::DrawSky(Camera camera) const {
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

void World::SetWeather(const WeatherState& w) {
    weather = w;
}