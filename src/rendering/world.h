#pragma once
#include "raylib.h"
#include "raymath.h"

// ---------------------------------------------------------------------------
// Sky color preset for a time of day
// ---------------------------------------------------------------------------
struct SkyPreset {
    Vector3 zenith;
    Vector3 horizon;
    Vector3 ground;
    Vector3 sunColor;
    float   sunIntensity;
    float   cloudOpacity;
};

// ---------------------------------------------------------------------------
// World namespace — single source of truth for all environmental state
// ---------------------------------------------------------------------------
namespace World {

    // -----------------------------------------------------------------------
    // Fog (unified — feeds both post-process AND sky shader)
    // -----------------------------------------------------------------------
    inline bool          fogEnabled     = true;
    inline float         fogDensity     = 0.05f;  // exponential density for post-process
    inline float         fogSkyAmount   = 0.85f;    // linear blend (0-1) for sky shader fog band
    inline float         fogStart       = 0.0f;   // fog-free zone (meters)
    inline float         fogMaxDist     = 50.0f;  // fully opaque beyond this
    inline float         fogHeightFade  = 35.0f;    // fade range above fogZHeight (meters)
    inline float         fogZHeight     = 5.0f;     // world Y below which fog is full
    inline float         fogDitherBlend = 0.0f;     // bayer dither at edges (0-1)
    inline float         fogGroundBlend = 1.0f;     // how much sky ground color tints fog
    inline float         fogNear        = 0.1f;     // camera near plane
    inline float         fogFar         = 550.0f;  // camera far plane
    inline float         fogHeight      = 0.6f;     // sky shader fog height band
    inline Color         fogColor       = {140, 142, 148, 255};
    inline Color         fogBaseColor   = {140, 142, 148, 255};
    inline float         fogSkyTint     = 0.55f;

    // Fog noise — procedural wisps
    inline float         fogNoiseScale    = 0.02f;  // world-space frequency of noise
    inline float         fogNoiseStrength = 0.4f;    // 0 = solid fog, 1 = fully broken up
    inline float         fogWindSpeed     = 0.1f;    // world units/sec noise scrolls
    inline Vector2       fogWindDir       = {1.0f, 0.3f}; // xz direction (auto-normalized)

    // -----------------------------------------------------------------------
    // Shadows
    // -----------------------------------------------------------------------
    inline float         shadowOpacity   = 0.65f;
    inline float         shadowSunOffset = 0.3f;
    inline float         shadowSizeScale = 2.5f;
    inline float         shadowMaxStretch= 2.0f;
    inline bool          shadowEnabled   = true;
    inline Vector3       shadowColor     = {0.0f, 0.0f, 0.0f};

    // -----------------------------------------------------------------------
    // Day cycle
    // -----------------------------------------------------------------------
    inline float         dayLengthSec     = 600.0f;
    inline float         timeScale        = 10.0f;
    inline float         startDayProgress = 0.0f;

    inline float         dawnWidth        = 0.1f;
    inline float         duskWidth        = 0.1f;
    inline Vector3       moonColor        = {0.15f, 0.18f, 0.35f};
    inline float         moonIntensity    = 5.0f;
    inline float         skylightHeight   = 0.5f;
    inline float         sunFadeStart     = 0.03f;
    inline float         sunFadeEnd       = 0.0f;

    // PBR lighting intensity (fed to shader as multipliers)
    inline float         sunPeakIntensity     = 5.0f;   // direct sun at noon
    inline float         skylightPeakIntensity= 6.0f;   // ambient skylight at noon
    inline float         nightSunIntensity    = 0.5f;  // moonlight direct
    inline float         nightSkylightIntensity= 5.0f;  // ambient at night



    // -----------------------------------------------------------------------
    // Sky sphere geometry
    // -----------------------------------------------------------------------
    inline constexpr float SKY_SPHERE_RADIUS = 500.0f;
    inline constexpr int   SKY_SPHERE_RINGS  = 16;
    inline constexpr int   SKY_SPHERE_SLICES = 16;

    // -----------------------------------------------------------------------
    // Asset paths
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    // Read-only computed state (written by Update)
    // -----------------------------------------------------------------------
    inline float         dayProgress  = 0.0f;
    inline float         worldTime    = 0.0f;
    inline SkyPreset     currentSky   = {};
    //inline float fogHeight = 0.0f
    // -----------------------------------------------------------------------
    // Weather (runtime-driven)
    // -----------------------------------------------------------------------
    inline float         rainAmount    = 0.0f;
    inline float         windStrength  = 1.0f;
    inline Vector3 windDirection = {1.0f,0.5f,0.2f}; //X,Y, freq

    // Lighting (computed by Update)
    inline Vector3       lightDir         = {};
    inline Vector3       lightColor       = {};
    inline float         lightIntensity   = 0.0f;
    inline Vector3       skylightColor    = {};
    inline float         skylightIntensity= 0.0f;

    inline Vector3       cameraPos    = {};
    inline Vector3       cameraFwd    = {};
    inline Vector3       cameraRight  = {};
    inline Vector3       cameraUp     = {};
    inline float         cameraFov    = 90.0f;

    // -----------------------------------------------------------------------
    // API
    // -----------------------------------------------------------------------
    void Init();
    void Unload();
    void Update(Camera camera);
    void DrawSky(Camera camera);

    void SetDayProgress(float t);
    void SetPreset(int index, const SkyPreset& preset);

    // CPU-side fog culling
    inline bool IsFogCulled(Vector3 camPos, Vector3 objPos, float objRadius = 0.0f) {
        if (!fogEnabled) return false;
        float dist = Vector3Distance(camPos, objPos) - objRadius;
        return dist > fogMaxDist;
    }

} // namespace World