#version 330

in vec2 fragUV;
out vec4 finalColor;

uniform sampler2D texture0;      // color buffer
uniform sampler2D depthTexture;  // depth buffer
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
uniform float fogHeightFade;   // reserved for ground-fog vertical fade
uniform float fogDitherBlend;  // bayer dither at fog boundary
uniform vec3  fogColor;
uniform float fogNear;
uniform float fogFar;

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

// --- Bayer 8x8 for finer fog dither ---
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

// Linearize non-linear depth buffer → view-space distance
float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0; // back to NDC
    return (2.0 * fogNear * fogFar) / (fogFar + fogNear - z * (fogFar - fogNear));
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

    vec3 color = texture(texture0, uv).rgb;

   if (fogEnabled > 0) {
        vec4 sceneData = texture(texture0, uv);
        color = sceneData.rgb;

        if (sceneData.a > 0.001) {
            float dist = sceneData.a * fogFar;
            float fogDist = max(dist - fogStart, 0.0);
            float exponent = fogDensity * fogDist;
            float fogFactor = 1.0 - exp(-exponent * exponent);
            fogFactor = mix(fogFactor, 1.0, step(fogMaxDist, dist));

            if (fogDitherBlend > 0.0) {
                float d = bayer8x8(ivec2(gl_FragCoord.xy));
                fogFactor += d * fogDitherBlend * 0.15;
            }

            fogFactor = clamp(fogFactor, 0.0, 1.0);
            color = mix(color, fogColor, fogFactor);
        }
    }
    // --- Color dither (existing) ---
    if (ditherEnabled > 0) {
        float d = bayer4x4(ivec2(gl_FragCoord.xy)) * ditherStrength / ditherColorDepth;
        color = floor(color * ditherColorDepth + 0.5 + d * ditherColorDepth) / ditherColorDepth;
    }

    finalColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}