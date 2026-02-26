#version 100
precision mediump float;

varying vec2 fragUV;


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
            gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }
    }

    vec4 sceneData = texture2D(texture0, uv);
    vec3 color = sceneData.rgb;

    if (fogEnabled > 0 && sceneData.a > 0.001) {
        float linearDist = sceneData.a * fogFar;
        // Reconstruct world position from camera ray + linear depth
        float aspect = resolution.x / resolution.y;
        float tanHalf = tan(camFov * 0.5);
        vec2 ndcXY = uv * 2.0 - 1.0;
        vec3 rayDir = normalize(camFwd
                    + ndcXY.x * aspect * tanHalf * camRight
                    + ndcXY.y * tanHalf * camUp);
        float rayDist = linearDist / max(dot(rayDir, camFwd), 0.001);
        vec3 worldPos = camPos + rayDir * rayDist;

        // Distance fog (exponential squared)
        float fogDist = max(rayDist - fogStart, 0.0);
        float exponent = fogDensity * fogDist;
        float fogFactor = 1.0 - exp(-exponent * exponent);
        fogFactor = mix(fogFactor, 1.0, step(fogMaxDist, linearDist));

        // Height fade: full below fogZHeight, fades over fogHeightFade range
        float heightFactor = 1.0 - smoothstep(fogZHeight, fogZHeight + fogHeightFade, worldPos.y);
        fogFactor *= heightFactor;

        // Procedural noise wisps — pans with wind
        if (fogNoiseStrength > 0.0, rayDist <fogFar) {
            vec2 noiseUV = worldPos.xz * fogNoiseScale + fogWindOffset;
            float n = fbm(noiseUV);
            float noiseMask = smoothstep(0.3 * fogNoiseStrength, 1.0 - 0.2 * fogNoiseStrength, n);
            fogFactor *= mix(1.0, noiseMask, fogNoiseStrength);
        }

        float fogFactorClean = fogFactor;

        if (fogDitherBlend > 0.0) {
            float n = fract(sin(dot(gl_FragCoord.xy + camPos.xz * 40.0, vec2(12.9898, 78.233))) * 13.79) - 0.5;
            fogFactor = clamp(fogFactor + n * 0.15, 0.0, 1.0);
        }
        fogFactorClean = clamp(fogFactorClean, 0.0, 1.0);

        vec3 fogClean    = mix(color, fogColor, fogFactorClean);
        vec3 fogDithered = mix(color, fogColor, fogFactor);
        color = mix(fogClean, fogDithered, fogDitherBlend);
    }

    if (ditherEnabled > 0) {
        float n = fract(sin(dot(gl_FragCoord.xy + camPos.xz * 40.0, vec2(12.98, 28.2))) * 17.3) - 0.5;
        vec3 dithered = floor(color * ditherColorDepth + 0.5 + n) / ditherColorDepth;
        color = mix(color, dithered, ditherStrength);
    }

    gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);

}
