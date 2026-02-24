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

out vec4 finalColor;

// --- Procedural noise for cloud wisps ---
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
        mix(hash(i), hash(i + vec2(1, 0)), f.x),
        mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x),
        f.y
    );
}

float fbm(vec2 p)
{
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 5; i++)
    {
        v += a * noise(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

void main()
{
    vec3 dir = normalize(fragWorldPos - viewPos);
    vec3 sun = normalize(-sunDir);

    // Height gradient: 0 at horizon, 1 at zenith
    float h = max(dir.y, 0.0);

    // Sky color from uniform presets
    vec3 sky = mix(skyHorizon, skyZenith, pow(h, 0.6));

    // Sun disc + glow (scaled by intensity for night fadeout)
    float sunDot = max(dot(dir, sun), 0.0);
    sky += sunColor * pow(sunDot, 256.0) * sunIntensity;       // hard disc
    sky += sunColor * 0.3 * pow(sunDot, 8.0) * sunIntensity;  // soft glow
    sky += sunColor * 0.1 * pow(sunDot, 2.0) * sunIntensity;  // atmospheric scatter

    // Sunset/sunrise tint near horizon
    float sunsetMask = pow(1.0 - h, 4.0) * pow(sunDot, 2.0);
    sky += sunColor * sunsetMask * 0.5 * sunIntensity;

    // Stars at night — cube-projected for uniform distribution, no horizon stretch
    {
        float nightFade = 1.0 - smoothstep(0.0, 0.4, sunIntensity);
        if (nightFade > 0.0 && dir.y > 0.0)
        {
            vec3 ad = abs(dir);
            float maxC = max(ad.x, max(ad.y, ad.z));
            // Project onto dominant cube face for uniform grid cells
            vec2 starUV;
            float faceId;
            if (maxC == ad.x)      { starUV = dir.yz / dir.x; faceId = 0.0; }
            else if (maxC == ad.y) { starUV = dir.xz / dir.y; faceId = 1.0; }
            else                   { starUV = dir.xy / dir.z; faceId = 2.0; }
            faceId += step(0.0, -dir.x) + step(0.0, -dir.y) * 2.0 + step(0.0, -dir.z) * 4.0;

            float starScale = 500.0;
            vec2 cell = floor(starUV * starScale);
            vec2 cellHash = vec2(hash(cell + faceId), hash(cell * 1.73 + faceId + 7.0));

            // Star presence + brightness variation
            float starOn = step(0.985, cellHash.x);
            float brightness = 0.5 + 0.5 * cellHash.y;

            // Flicker: per-star phase, varying speed
            float phase = cellHash.x * 6283.0;
            float speed = 1.5 + cellHash.y * 2.5;
            float flicker = 0.7 + 0.3 * sin(time * speed + phase);

            // Compact point: distance from cell center for soft falloff
            vec2 cellFrac = fract(starUV * starScale) - 0.5;
            float dist = length(cellFrac);
            float point = smoothstep(0.12, 0.02, dist);

            sky += vec3(starOn * point * brightness * flicker * nightFade * 0.9);
        }
    }

    // Procedural clouds
    if (h > 0.01)
    {
        vec2 cloudUV = dir.xz / (dir.y + 0.3) * 2.0;
        cloudUV += time * 0.02;
        float clouds = fbm(cloudUV);
        clouds = smoothstep(0.4, 0.7, clouds);

        // Cloud color (lit by sun)
        vec3 cloudColor = mix(skyHorizon * 0.8, sunColor * 0.9, pow(sunDot, 2.0) * sunIntensity);
        // At night, clouds are dark silhouettes
        cloudColor = mix(skyZenith * 0.3, cloudColor, sunIntensity);

        sky = mix(sky, cloudColor, clouds * cloudOpacity * smoothstep(0.0, 0.2, h));
    }

    // Below horizon: ground color
    if (dir.y < 0.0)
    {
        sky = mix(skyHorizon, skyGround, min(-dir.y * 5.0, 1.0));
    }

    // Fog: flat band based on view direction elevation
    if (fogAmount > 0.0)
    {
        float normalizedH = dir.y / max(fogHeight, 0.01);
        float fogMask = 1.0 - smoothstep(0.0, 1.0, max(normalizedH, 0.0));

        if (dir.y < 0.0) fogMask = 1.0;

        sky = mix(sky, fogColor, fogMask * fogAmount);
    }

    finalColor = vec4(sky, 0.0);  // alpha 0 = sky pixel
}