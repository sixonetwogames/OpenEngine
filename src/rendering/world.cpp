#include "world.h"
#include "rlgl.h"
#include <cmath>

#include "shader_utils.h"
// ---------------------------------------------------------------------------
// Internal state (file-scope, not exposed in header)
// ---------------------------------------------------------------------------
namespace {
    Model     skyModel   = {};
    Shader    skyShader  = {};
    Texture2D cloudTex   = {};
    int       cloudTexSlot = 1; // texture unit for cloud sampler

    // --- Uniform locations ---
    int timeLoc         = -1;
    int viewPosLoc      = -1;
    int sunDirLoc       = -1;
    int sunColorLoc     = -1;
    int sunIntensityLoc = -1;
    int skyZenithLoc    = -1;
    int skyHorizonLoc   = -1;
    int skyGroundLoc    = -1;
    int cloudOpacityLoc = -1;

    // Fog
    int fogColorLoc_sky         = -1;
    int fogAmountLoc_sky        = -1;
    int fogHeightLoc_sky        = -1;
    int fogNoiseScaleLoc_sky    = -1;
    int fogNoiseStrengthLoc_sky = -1;
    int fogWindOffsetLoc_sky    = -1;

    // Sun disc & halo
    int sunDiskSizeLoc          = -1;
    int sunDiskIntensityLoc     = -1;
    int sunHaloSizeLoc          = -1;
    int sunHaloIntensityLoc     = -1;
    int sunHaloMixLoc           = -1;
    int sunScatterSizeLoc       = -1;
    int sunScatterIntensityLoc  = -1;

    // Cloud texture params
    int cloudTexLoc             = -1;
    int cloudScaleLoc           = -1;
    int cloudSpeedLoc           = -1;
    int cloudDetailScaleLoc     = -1;
    int cloudDetailSpeedRatioLoc= -1;
    int cloudCoverageScaleLoc   = -1;
    int cloudCoverageSpeedLoc   = -1;
    int cloudDensityThresholdLoc= -1;
    int cloudDensitySmoothLoc   = -1;
    int cloudErosionStrengthLoc = -1;
    int cloudHeightFadeLoc      = -1;
    int cloudWindDirLoc         = -1;

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
        .sunIntensity = 0.0f, .cloudOpacity = 0.7f,
    };
    presets[1] = { // Dawn
        .zenith = {0.15f, 0.15f, 0.35f}, .horizon = {0.85f, 0.45f, 0.25f},
        .ground = {0.12f, 0.10f, 0.08f}, .sunColor = {1.0f, 0.6f, 0.3f},
        .sunIntensity = 0.4f, .cloudOpacity = 0.9f,
    };
    presets[2] = { // Day
        .zenith = {0.15f, 0.25f, 0.55f}, .horizon = {0.60f, 0.70f, 0.85f},
        .ground = {0.20f, 0.25f, 0.20f}, .sunColor = {1.0f, 0.95f, 0.85f},
        .sunIntensity = 1.0f, .cloudOpacity = 0.6f,
    };
    presets[3] = { // Sunset
        .zenith = {0.12f, 0.10f, 0.30f}, .horizon = {0.90f, 0.35f, 0.15f},
        .ground = {0.10f, 0.08f, 0.06f}, .sunColor = {1.0f, 0.5f, 0.2f},
        .sunIntensity = 0.4f, .cloudOpacity = 0.9f,
    };
    presets[4] = { // Night (wrap)
        .zenith = {0.07f, 0.09f, 0.3f}, .horizon = {0.21f, 0.24f, 0.62f},
        .ground = {0.01f, 0.01f, 0.03f}, .sunColor = {0.3f, 0.3f, 0.5f},
        .sunIntensity = 0.0f, .cloudOpacity = 0.7f,
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

    float blend = Clamp01((sunElev + 0.15f) / 0.3f);
    blend = blend * blend * (3.0f - 2.0f * blend);

    float dayFactor = Clamp01(sunElev / 0.5f);
    dayFactor = dayFactor * dayFactor * (3.0f - 2.0f * dayFactor);

    float sunIntensity  = World::currentSky.sunIntensity * blend *
                          (World::nightSunIntensity + (World::sunPeakIntensity - World::nightSunIntensity) * dayFactor);
    float moonIntensity = World::moonIntensity * (1.0f - blend) * World::nightSunIntensity;

    float effectiveAngle = sunAngle + (1.0f - blend) * PI;
    World::lightDir   = Vector3Normalize({cosf(effectiveAngle), -sinf(effectiveAngle), -0.3f});
    World::lightColor = V3Lerp(World::moonColor, World::currentSky.sunColor, blend);
    World::lightIntensity = sunIntensity + moonIntensity;

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

    Vector3 base = {
        World::fogBaseColor.r / 255.0f,
        World::fogBaseColor.g / 255.0f,
        World::fogBaseColor.b / 255.0f
    };

    base = V3Lerp(base, sky.ground, World::fogGroundBlend);

    base.x += skylight.x * World::fogSkyTint;
    base.y += skylight.y * World::fogSkyTint;
    base.z += skylight.z * World::fogSkyTint;

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
// Helper: cache a shader uniform location
// ---------------------------------------------------------------------------
static int Loc(Shader s, const char* name) {
    return GetShaderLocation(s, name);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void World::Init() {
    InitDefaultPresets();
    dayProgress = startDayProgress;

    // Sky sphere
    Mesh mesh = GenMeshSphere(SKY_SPHERE_RADIUS, SKY_SPHERE_RINGS, SKY_SPHERE_SLICES);
    skyModel  = LoadModelFromMesh(mesh);
    skyShader = LoadPlatformShader("sky");
    skyModel.materials[0].shader = skyShader;

    // Cloud texture (RGBA channel-packed)
    cloudTex = LoadTexture(cloudTexturePath);
    SetTextureFilter(cloudTex, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(cloudTex, TEXTURE_WRAP_REPEAT);

    // --- Cache all uniform locations ---
    timeLoc         = Loc(skyShader, "time");
    viewPosLoc      = Loc(skyShader, "viewPos");
    sunDirLoc       = Loc(skyShader, "sunDir");
    sunColorLoc     = Loc(skyShader, "sunColor");
    sunIntensityLoc = Loc(skyShader, "sunIntensity");
    skyZenithLoc    = Loc(skyShader, "skyZenith");
    skyHorizonLoc   = Loc(skyShader, "skyHorizon");
    skyGroundLoc    = Loc(skyShader, "skyGround");
    cloudOpacityLoc = Loc(skyShader, "cloudOpacity");

    // Fog
    fogColorLoc_sky         = Loc(skyShader, "fogColor");
    fogAmountLoc_sky        = Loc(skyShader, "fogAmount");
    fogHeightLoc_sky        = Loc(skyShader, "fogHeight");
    fogNoiseScaleLoc_sky    = Loc(skyShader, "fogNoiseScale");
    fogNoiseStrengthLoc_sky = Loc(skyShader, "fogNoiseStrength");
    fogWindOffsetLoc_sky    = Loc(skyShader, "fogWindOffset");

    // Sun disc & halo
    sunDiskSizeLoc          = Loc(skyShader, "sunDiskSize");
    sunDiskIntensityLoc     = Loc(skyShader, "sunDiskIntensity");
    sunHaloSizeLoc          = Loc(skyShader, "sunHaloSize");
    sunHaloIntensityLoc     = Loc(skyShader, "sunHaloIntensity");
    sunHaloMixLoc           = Loc(skyShader, "sunHaloMix");
    sunScatterSizeLoc       = Loc(skyShader, "sunScatterSize");
    sunScatterIntensityLoc  = Loc(skyShader, "sunScatterIntensity");

    // Cloud texture params
    cloudTexLoc              = Loc(skyShader, "cloudTex");
    cloudScaleLoc            = Loc(skyShader, "cloudScale");
    cloudSpeedLoc            = Loc(skyShader, "cloudSpeed");
    cloudDetailScaleLoc      = Loc(skyShader, "cloudDetailScale");
    cloudDetailSpeedRatioLoc = Loc(skyShader, "cloudDetailSpeedRatio");
    cloudCoverageScaleLoc    = Loc(skyShader, "cloudCoverageScale");
    cloudCoverageSpeedLoc    = Loc(skyShader, "cloudCoverageSpeed");
    cloudDensityThresholdLoc = Loc(skyShader, "cloudDensityThreshold");
    cloudDensitySmoothLoc    = Loc(skyShader, "cloudDensitySmooth");
    cloudErosionStrengthLoc  = Loc(skyShader, "cloudErosionStrength");
    cloudHeightFadeLoc       = Loc(skyShader, "cloudHeightFade");
    cloudWindDirLoc          = Loc(skyShader, "cloudWindDir");

    // Bind cloud texture sampler to its slot (one-time)
    SetShaderValue(skyShader, cloudTexLoc, &cloudTexSlot, SHADER_UNIFORM_INT);
}

void World::Unload() {
    UnloadTexture(cloudTex);
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

    // --- Sky shader uniforms ---
    float sunAngle = (dayProgress - 0.25f) * PI * 2.0f;
    Vector3 skyShaderSunDir = Vector3Normalize({cosf(sunAngle), -sinf(sunAngle), -0.3f});

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

    // Camera state for post-process
    cameraPos   = pos;
    cameraFwd   = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    cameraRight = Vector3Normalize(Vector3CrossProduct(cameraFwd, camera.up));
    cameraUp    = Vector3CrossProduct(cameraRight, cameraFwd);
    cameraFov   = camera.fovy;

    // Sun disc & halo
    SetShaderValue(skyShader, sunDiskSizeLoc,         &sunDiskSize,         SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, sunDiskIntensityLoc,    &sunDiskIntensity,    SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, sunHaloSizeLoc,         &sunHaloSize,         SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, sunHaloIntensityLoc,    &sunHaloIntensity,    SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, sunHaloMixLoc,          &sunHaloMix,          SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, sunScatterSizeLoc,      &sunScatterSize,      SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, sunScatterIntensityLoc, &sunScatterIntensity, SHADER_UNIFORM_FLOAT);

    // Cloud texture params
    SetShaderValue(skyShader, cloudScaleLoc,            &cloudScale,            SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, cloudSpeedLoc,            &cloudSpeed,            SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, cloudDetailScaleLoc,      &cloudDetailScale,      SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, cloudDetailSpeedRatioLoc, &cloudDetailSpeedRatio, SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, cloudCoverageScaleLoc,    &cloudCoverageScale,    SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, cloudCoverageSpeedLoc,    &cloudCoverageSpeed,    SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, cloudDensityThresholdLoc, &cloudDensityThreshold, SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, cloudDensitySmoothLoc,    &cloudDensitySmooth,    SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, cloudErosionStrengthLoc,  &cloudErosionStrength,  SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, cloudHeightFadeLoc,       &cloudHeightFade,       SHADER_UNIFORM_FLOAT);

    // Normalize and upload cloud wind direction
    float cwLen = sqrtf(cloudWindDir.x * cloudWindDir.x + cloudWindDir.y * cloudWindDir.y);
    float cwN[2] = {0.0f, 0.0f};
    if (cwLen > 0.001f) { cwN[0] = cloudWindDir.x / cwLen; cwN[1] = cloudWindDir.y / cwLen; }
    SetShaderValue(skyShader, cloudWindDirLoc, cwN, SHADER_UNIFORM_VEC2);

    // Fog → sky shader
    float fogCol[3] = { fogColor.r/255.0f, fogColor.g/255.0f, fogColor.b/255.0f };
    SetShaderValue(skyShader, fogHeightLoc_sky, &fogHeight,    SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, fogColorLoc_sky,  fogCol,        SHADER_UNIFORM_VEC3);
    SetShaderValue(skyShader, fogAmountLoc_sky, &fogSkyAmount, SHADER_UNIFORM_FLOAT);

    // Fog noise → sky shader
    SetShaderValue(skyShader, fogNoiseScaleLoc_sky,    &fogNoiseScale,    SHADER_UNIFORM_FLOAT);
    SetShaderValue(skyShader, fogNoiseStrengthLoc_sky, &fogNoiseStrength, SHADER_UNIFORM_FLOAT);
    float wLen = sqrtf(fogWindDir.x * fogWindDir.x + fogWindDir.y * fogWindDir.y);
    float wNrm = (wLen > 0.001f) ? 1.0f / wLen : 0.0f;
    float windOff[2] = {
        fogWindDir.x * wNrm * fogWindSpeed * worldTime,
        fogWindDir.y * wNrm * fogWindSpeed * worldTime
    };
    SetShaderValue(skyShader, fogWindOffsetLoc_sky, windOff, SHADER_UNIFORM_VEC2);
}

void World::DrawSky(Camera camera) {
    rlDisableBackfaceCulling();
    rlDisableDepthMask();

        // Bind cloud texture to its slot before drawing
        rlActiveTextureSlot(cloudTexSlot);
        rlEnableTexture(cloudTex.id);

        DrawModel(skyModel, camera.position, 1.0f, WHITE);

        rlActiveTextureSlot(cloudTexSlot);
        rlDisableTexture();

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