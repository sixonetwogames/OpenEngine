#version 100
#extension GL_OES_standard_derivatives : enable
precision mediump float;
precision mediump int;

varying vec2  fragTexCoord;
varying vec3  fragWorldPos;
varying vec3  fragNormal;
varying vec2  fragLocalCoord;
varying float fragFog;
varying vec3  fragFogColor;

uniform sampler2D texture0;

uniform float alphaThreshold;
uniform vec4  tintColor;

uniform vec3  sunDir;
uniform vec3  sunColor;
uniform vec3  ambientColor;

uniform vec3  cameraPos;
uniform vec3  billboardPos;

uniform float roughness;
uniform float metallic;
uniform float normalStrength;

uniform int   spherical;
uniform float sphereSpeed;

uniform float time;


// ─── Procedural noise ───────────────────────────────────────────────────────

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float vnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i),              hash(i + vec2(1.0, 0.0)), f.x),
               mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

float fbm(vec2 p, int octaves) {
    float v = 0.0, a = 0.5;
    mat2 rot = mat2(0.8, 0.6, -0.6, 0.8);
    for (int i = 0; i < 4; i++) {
        if (i >= octaves) break;
        v += a * vnoise(p);
        p = rot * p * 2.0;
        a *= 0.5;
    }
    return v;
}

// ─── Sphere rendering ───────────────────────────────────────────────────────

const float PI = 3.14159265;

void main()
{
    if (spherical == 1) {
        // ── Disc → sphere mapping ───────────────────────────────────
        vec2 p = fragLocalCoord * 2.0;           // -1..1
        float r2 = dot(p, p);
        if (r2 > 1.0) discard;

        float z = sqrt(1.0 - r2);
        vec3 sphereNorm = vec3(p.x, p.y, z);     // local-space sphere normal

        // ── Transform to world space, then project UVs ───────────
        vec3 look     = normalize(cameraPos - billboardPos);
        vec3 camRight = normalize(cross(vec3(0.0, 1.0, 0.0), look));
        vec3 camUp    = cross(look, camRight);

        // World-space point on sphere surface
        vec3 worldPt = sphereNorm.x * camRight + sphereNorm.y * camUp + sphereNorm.z * look;

        // Time-based rotation around world Y axis
        float angle = time * sphereSpeed;
        float ca = cos(angle), sa = sin(angle);
        vec3 rotPt = vec3(
            worldPt.x * ca + worldPt.z * sa,
            worldPt.y,
           -worldPt.x * sa + worldPt.z * ca
        );

        // ── Spherical UV from world-space rotated point ───────────
        float u = atan(rotPt.x, rotPt.z) / (2.0 * PI) + 0.5;
        float v = asin(clamp(rotPt.y, -1.0, 1.0)) / PI + 0.5;
        vec2 sphereUV = vec2(u, v);

        // ── Noise perturbation for surface detail ───────────────────
        float n  = fbm(sphereUV * 8.0 + vec2(time * 0.02, 0.0), 4);
        float n2 = fbm(sphereUV * 16.0, 3);
        sphereUV += vec2(n - 0.5, n2 - 0.5) * 0.03;

        vec4 texel = texture2D(texture0, sphereUV);
        vec3 albedo = texel.rgb * tintColor.rgb;

        // Noise-based surface variation
        float detail = fbm(sphereUV * 12.0 + vec2(0.0, time * 0.01), 3);
        albedo *= 0.85 + 0.3 * detail;

        // ── World-space normal (reuses basis vectors from UV projection) ──
        vec3 N = normalize(sphereNorm.x * camRight + sphereNorm.y * camUp + sphereNorm.z * look);

        // ── Bump from noise ─────────────────────────────────────────
        if (normalStrength > 0.0) {
            float eps = 0.01;
            float nh  = fbm(sphereUV * 8.0, 3);
            float nhx = fbm((sphereUV + vec2(eps, 0.0)) * 8.0, 3);
            float nhy = fbm((sphereUV + vec2(0.0, eps)) * 8.0, 3);
            vec3 bumpT = normalize(camRight + N * normalStrength * (nhx - nh) / eps);
            vec3 bumpB = normalize(camUp    + N * normalStrength * (nhy - nh) / eps);
            N = normalize(cross(bumpT, bumpB));
            // Ensure normal faces camera
            if (dot(N, look) < 0.0) N = -N;
        }

        // ── Lighting: radial mask + mix ──────────────────────────────
        vec3 L = normalize(-sunDir);
        float NdotL = dot(N, L);
        float diff  = NdotL * 0.5 + 0.5;

        // Radial mask: tight falloff from sunlit side
        float mask = smoothstep(0.0, 1.0, diff * diff);

        // Specular
        vec3  V     = normalize(cameraPos - fragWorldPos);
        vec3  H     = normalize(L + V);
        float NdotH = max(dot(N, H), 0.0);
        float r4    = roughness * roughness * roughness * roughness;
        float spec  = pow(NdotH, 2.0 / max(r4, 0.001) - 2.0);
        vec3  specC  = mix(vec3(0.04), albedo, metallic);

        // Mix ambient base with sunlit color via radial mask
        vec3 ambient = albedo * (ambientColor * 0.3);
        vec3 sunlit  = albedo * sunColor * 0.5 + specC * sunColor * spec * 0.3;
        vec3 color   = mix(ambient, sunlit, mask);

        // ── Bottom darken ────────────────────────────────────────────
        float bottomFade = smoothstep(0.0, -1.0, p.y);
        color = mix(color, vec3(0.02), bottomFade);

        // ── Fog ─────────────────────────────────────────────────────
        color = mix(color, fragFogColor, fragFog);

        gl_FragColor = vec4(color, 0.0);

    } else {
        // ── Original flat billboard path ────────────────────────────
        vec4 texel = texture2D(texture0, fragTexCoord);
        if (texel.a < alphaThreshold) discard;

        vec3  albedo = texel.rgb * tintColor.rgb;

        vec3 N = normalize(fragNormal);
        if (normalStrength > 0.0) {
            float lum  = dot(texel.rgb, vec3(0.299, 0.587, 0.114));
            float dldx = dFdx(lum);
            float dldy = dFdy(lum);
            vec3  T    = normalize(dFdx(fragWorldPos));
            vec3  B    = normalize(dFdy(fragWorldPos));
            N = normalize(N + normalStrength * (-dldx * T - dldy * B));
        }

        vec3 L = normalize(-sunDir);
        float NdotL = dot(N, L);
        float diff  = NdotL * 0.5 + 0.5;

        // Radial mask: tight falloff from sunlit side
        float mask = smoothstep(0.0, 1.0, diff * diff);

        // Mix ambient base with sunlit color via radial mask
        vec3 ambient = albedo * (ambientColor * 0.3);
        vec3 sunlit  = albedo * sunColor * 0.5;
        vec3 color   = mix(ambient, sunlit, mask);

        color = mix(color, fragFogColor, fragFog);

        gl_FragColor = vec4(color, 0.0);
    }
}