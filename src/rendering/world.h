#pragma once
#include "raylib.h"
#include "raymath.h"

// --- Sky color preset for a time of day ---
struct SkyPreset {
    Vector3 zenith;
    Vector3 horizon;
    Vector3 ground;
    Vector3 sunColor;
    float   sunIntensity;
    float   cloudOpacity;
};

struct WeatherState {
    float fogDensity   = 0.4f;
    float rainAmount   = 0.0f;
    float windStrength = 0.0f;
    Color  fogColor    = {180, 190, 200, 255};
};

struct DayCycleSettings {
    // Dawn/dusk width as fraction of day (smaller = shorter transitions)
    float dawnWidth     = 0.1f;   // was effectively 0.25, now ~36s of a 600s day
    float duskWidth     = 0.1f;

    // Moon (nighttime directional light)
    Vector3 moonColor   = {0.15f, 0.18f, 0.35f};  // blue tone
    float   moonIntensity = 0.32f;

    // Skylight (ambient from sky color)
    float skylightHeight    = 0.5f;    // 0 = horizon, 1 = zenith — where to sample
    float skylightIntensity = 5.0f;   // multiplier on sky-sampled ambient

    // Sun horizon fade
    float sunFadeStart = 0.08f;  // elevation where sun starts fading (radians-ish)
    float sunFadeEnd   = 0.0f;   // elevation where sun is fully gone
};

// Computed lighting state each frame
struct LightingState {
    Vector3 lightDir;        // current active light direction (sun or moon)
    Vector3 lightColor;      // sun or moon color
    float   lightIntensity;  // sun or moon intensity
    Vector3 skylightColor;   // ambient tint from sky
    float   skylightIntensity;
};

class World {
public:
    void Init();
    void Unload();
    void Update(Camera camera);
    void DrawSky(Camera camera) const;

    // Time of day
    float GetDayProgress() const { return dayProgress; }
    void  SetDayProgress(float t);
    void  SetTimeScale(float scale) { timeScale = scale; }
    void  SetDayLengthSeconds(float secs) { dayLength = secs; }
    float GetWorldTime() const { return worldTime; }

    // Lighting getters (use these in Level::UpdateLighting)
    const LightingState& GetLighting() const { return lighting; }

    // Settings
    DayCycleSettings& GetDayCycleSettings() { return dayCycle; }

    // Override default presets
    void SetPreset(int index, const SkyPreset& preset);

    // Weather
    void SetWeather(const WeatherState& weather);
    const WeatherState& GetWeather() const { return weather; }

private:
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

    //TIME
    float   worldTime   = 0.0f;
    float   dayProgress = 0.0f;
    float   timeScale   = 13.0f;
    float   dayLength   = 600.0f;

    //FOG
    int fogColorLoc_sky    = -1;
    int fogAmountLoc_sky   = -1;
    int fogHeightLoc_sky   = -1;


    //PRESETS
    SkyPreset presets[4] = {};
    SkyPreset currentSky; 

    //WEATHER/LIGHTING
    DayCycleSettings dayCycle;
    LightingState    lighting = {};

    WeatherState weather = {};

    void InitDefaultPresets();
    SkyPreset BlendPresets(float t) const;
    float RemapDayProgress(float t) const;
    void  ComputeLighting(float t);
};