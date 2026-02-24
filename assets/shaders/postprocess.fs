#version 330

in vec2 fragUV;
out vec4 finalColor;

uniform sampler2D texture0;
uniform sampler2D depthTexture;
uniform vec2      resolution;

// Barrel Distortion
uniform int   barrelEnabled;
uniform float barrelStrength;
uniform float barrelZoom;

// Dither
uniform int   ditherEnabled;
uniform float ditherStrength;
uniform float ditherColorDepth;

// Fog
uniform int   fogEnabled;
uniform float fogDensity;
uniform float fogStart;
uniform float fogMaxDist;
uniform float fogHeightFade;
uniform float fogZHeight;
uniform float fogDitherBlend;
uniform vec3  fogColor;
uniform float fogNear;
uniform float fogFar;

// Camera for world reconstruction
uniform vec3  camPos;
uniform vec3  camFwd;
uniform vec3  camRight;
uniform vec3  camUp;
uniform float camFov;

// Fog noise
uniform float fogNoiseScale;
uniform float fogNoiseStrength;
uniform vec2  fogWindOffset;
uniform float fogTime;

// --- Bayer 4x4 ---
float bayer4x4(ivec2 p) {
    int b[16] = int[16](
         0,  8,  2, 10,
        12,  4, 14,  6,
         3, 11,  1,  9,
        15,  7, 13,  5
    );
    return float(b[(p.y % 4) * 4 + (p.x % 4)]) / 16.0 - 0.5;
}

// --- Bayer 8x8 ---
float bayer8x8(ivec2 p) {
    int b[64] = int[64](
         0, 32,  8, 40,  2, 34, 10, 42,
        48, 16, 56, 24, 50, 18, 58, 26,
        12, 44,  4, 36, 14, 46,  6, 38,
        60, 28, 52, 20, 62, 30, 54, 22,
         3, 35, 11, 43,  1, 33,  9, 41,
        51, 19, 59, 27, 49, 17, 57, 25,
        15, 47,  7, 39, 13, 45,  5, 37,
        63, 31, 55, 23, 61, 29, 53, 21
    );
    return float(b[(p.y % 8) * 8 + (p.x % 8)]) / 64.0 - 0.5;
}

// --- Procedural noise for fog wisps ---
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(hash(i), hash(i + vec2(1, 0)), f.x),
        mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x),
        f.y
    );
}

float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) {
        v += a * noise(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

vec2 barrelDistort(vec2 uv) {
    vec2 c = uv * 2.0 - 1.0;
    float aspect = resolution.x / resolution.y;
    c.x *= aspect;
    c *= (1.0 + barrelStrength * dot(c, c)) / barrelZoom;
    c.x /= aspect;
    return c * 0.5 + 0.5;
}

void main() {
    vec2 uv = fragUV;

    if (barrelEnabled > 0) {
        uv = barrelDistort(uv);
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
            finalColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }
    }

    vec4 sceneData = texture(texture0, uv);
    vec3 color = sceneData.rgb;

    if (fogEnabled > 0 && sceneData.a > 0.001) {
        float linearDist = sceneData.a * fogFar;

        // Reconstruct world position from camera ray + linear depth
        float aspect = resolution.x / resolution.y;
        float tanHalf = tan(camFov * 0.5);
        vec2 ndcXY = uv * 2.0 - 1.0;
        vec3 rayDir = camFwd
                    + ndcXY.x * aspect * tanHalf * camRight
                    + ndcXY.y * tanHalf * camUp;
        vec3 worldPos = camPos + rayDir * linearDist;

        // Distance fog (exponential squared)
        float fogDist = max(linearDist - fogStart, 0.0);
        float exponent = fogDensity * fogDist;
        float fogFactor = 1.0 - exp(-exponent * exponent);
        fogFactor = mix(fogFactor, 1.0, step(fogMaxDist, linearDist));

        // Height fade: full below fogZHeight, fades over fogHeightFade range
        float heightFactor = 1.0 - smoothstep(fogZHeight, fogZHeight + fogHeightFade, worldPos.y);
        fogFactor *= heightFactor;

        // Procedural noise wisps — pans with wind
        if (fogNoiseStrength > 0.0) {
            vec2 noiseUV = worldPos.xz * fogNoiseScale + fogWindOffset;
            float n = fbm(noiseUV);
            float noiseMask = smoothstep(0.3 * fogNoiseStrength, 1.0 - 0.2 * fogNoiseStrength, n);
            fogFactor *= mix(1.0, noiseMask, fogNoiseStrength);
        }

        // Bayer dither at fog edges
        if (fogDitherBlend > 0.0) {
            fogFactor += bayer8x8(ivec2(gl_FragCoord.xy)) * fogDitherBlend * 0.15;
        }

        fogFactor = clamp(fogFactor, 0.0, 1.0);
        color = mix(color, fogColor, fogFactor);
    }

    // Color dither
    if (ditherEnabled > 0) {
        float d = bayer4x4(ivec2(gl_FragCoord.xy)) * ditherStrength / ditherColorDepth;
        color = floor(color * ditherColorDepth + 0.5 + d * ditherColorDepth) / ditherColorDepth;
    }

    finalColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}