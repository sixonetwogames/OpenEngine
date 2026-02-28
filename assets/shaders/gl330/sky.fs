#version 330

in vec3 fragWorldPos;

uniform vec3  viewPos;
uniform vec3  sunDir;
uniform vec3  sunColor;
uniform float sunIntensity;
uniform vec3  skyZenith;
uniform vec3  skyHorizon;
uniform vec3  skyGround;
uniform float cloudOpacity;
uniform float time;

uniform vec3  fogColor;
uniform float fogAmount;
uniform float fogHeight;

// Fog noise
uniform float fogNoiseScale;
uniform float fogNoiseStrength;
uniform vec2  fogWindOffset;

// Sun disc & halo
uniform float sunDiskSize;
uniform float sunDiskIntensity;
uniform float sunHaloSize;
uniform float sunHaloIntensity;
uniform float sunHaloMix;
uniform float sunScatterSize;
uniform float sunScatterIntensity;

// Cloud texture (channel-packed fake 3D)
uniform sampler2D cloudTex;       // R=density, G=detail, B=coverage, A=erosion
uniform float cloudScale;
uniform float cloudSpeed;
uniform float cloudDetailScale;
uniform float cloudDetailSpeedRatio;
uniform float cloudCoverageScale;
uniform float cloudCoverageSpeed;
uniform float cloudDensityThreshold;
uniform float cloudDensitySmooth;
uniform float cloudErosionStrength;
uniform float cloudHeightFade;
uniform vec2  cloudWindDir;       // normalized scroll direction

out vec4 finalColor;

// --- Procedural noise (kept for fog wisps / stars) ---
float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x),
        mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x),
        f.y
    );
}

void main()
{
    vec3 dir = normalize(fragWorldPos - viewPos);
    vec3 sun = normalize(-sunDir);

    float h = max(dir.y, 0.0);

    // Sky gradient
    vec3 sky = mix(skyHorizon, skyZenith, pow(h, 0.6));

    // ---- Sun disc + halo with radial sky-color bleed ----
    float sunDot = max(dot(dir, sun), 0.0);

    // Hard disc
    sky += sunColor * pow(sunDot, sunDiskSize) * sunDiskIntensity * sunIntensity;

    // Soft halo — blends sun color toward sky color at edges
    vec3 haloColor = mix(sunColor, sky, sunHaloMix * (1.0 - pow(sunDot, sunHaloSize * 0.5)));
    sky += haloColor * pow(sunDot, sunHaloSize) * sunHaloIntensity * sunIntensity;

    // Atmospheric scatter ring
    sky += sunColor * pow(sunDot, sunScatterSize) * sunScatterIntensity * sunIntensity;

    // Sunset/sunrise horizon tint
    float sunsetMask = pow(1.0 - h, 4.0) * pow(sunDot, 2.0);
    sky += sunColor * sunsetMask * 0.5 * sunIntensity;

    // Stars at night — cube-projected
    {
        float nightFade = 1.0 - smoothstep(0.0, 0.4, sunIntensity);
        if (nightFade > 0.0 && dir.y > 0.0)
        {
            vec3 ad = abs(dir);
            float maxC = max(ad.x, max(ad.y, ad.z));
            vec2 starUV;
            float faceId;
            if (maxC == ad.x)      { starUV = dir.yz / dir.x; faceId = 0.0; }
            else if (maxC == ad.y) { starUV = dir.xz / dir.y; faceId = 1.0; }
            else                   { starUV = dir.xy / dir.z; faceId = 2.0; }
            faceId += step(0.0, -dir.x) + step(0.0, -dir.y) * 2.0 + step(0.0, -dir.z) * 4.0;

            float starScale = 500.0;
            vec2 cell = floor(starUV * starScale);
            vec2 cellHash = vec2(hash(cell + faceId), hash(cell * 1.73 + faceId + 7.0));

            float starOn = step(0.985, cellHash.x);
            float brightness = 0.5 + 0.5 * cellHash.y;
            float phase = cellHash.x * 6283.0;
            float speed = 1.5 + cellHash.y * 2.5;
            float flicker = 0.7 + 0.3 * sin(time * speed + phase);

            vec2 cellFrac = fract(starUV * starScale) - 0.5;
            float dist = length(cellFrac);
            float point = smoothstep(0.12, 0.02, dist);

            sky += vec3(starOn * point * brightness * flicker * nightFade * 0.9);
        }
    }

    // ---- Texture-based clouds (channel-packed fake 3D) ----
    if (h > 0.01)
    {
        vec2 baseUV = dir.xz / (dir.y + 0.3);

        // Per-layer scroll offsets (speed separation = fake depth)
        vec2 scrollBase     = cloudWindDir * time * cloudSpeed;
        vec2 scrollDetail   = cloudWindDir * time * cloudSpeed * cloudDetailSpeedRatio;
        vec2 scrollCoverage = cloudWindDir * time * cloudCoverageSpeed;

        // Sample each channel at its own UV scale + scroll
        float baseDensity = texture(cloudTex, baseUV * cloudScale + scrollBase).r;
        float detail      = texture(cloudTex, baseUV * cloudScale * cloudDetailScale + scrollDetail).g;
        float coverage    = texture(cloudTex, baseUV * cloudScale * cloudCoverageScale + scrollCoverage).b;
        float erosion     = texture(cloudTex, baseUV * cloudScale * cloudDetailScale + scrollDetail * 0.7).a;

        // Composite: base × coverage, blend in detail
        float density = baseDensity * coverage;
        density = density * 0.7 + detail * 0.3;
        density = smoothstep(cloudDensityThreshold, cloudDensityThreshold + cloudDensitySmooth, density);

        // Erode edges via alpha channel — core stays solid
        float edgeMask = smoothstep(0.0, 0.4, density);
        density *= mix(1.0, erosion, cloudErosionStrength * (1.0 - edgeMask));

        // Horizon fade (avoid projection stretch artifacts)
        float horizonFade = smoothstep(0.0, cloudHeightFade, h);

        // Cloud lighting
        vec3 cloudColor = mix(skyHorizon * 0.5, sunColor * 0.5, pow(sunDot, 2.0) * sunIntensity);
        cloudColor = mix(skyZenith * 0.3, cloudColor, sunIntensity);

        sky = mix(sky, cloudColor, density * cloudOpacity * horizonFade);
    }

    // Below horizon: ground
    if (dir.y < 0.0)
    {
        sky = mix(skyHorizon, skyGround, min(-dir.y * 5.0, 1.0));
    }

    // Fog band
    if (fogAmount > 0.0)
    {
        float normalizedH = dir.y / max(fogHeight, 0.01);
        float fogMask = 1.0 - smoothstep(0.0, 1.0, max(normalizedH, 0.0));
        if (dir.y < 0.0) fogMask = 1.0;
        sky = mix(sky, fogColor, fogMask * fogAmount);
    }

    finalColor = vec4(sky, 0.0);
}