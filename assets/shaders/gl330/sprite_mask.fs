#version 330

in vec2  fragTexCoord;
in vec3  fragWorldPos;
in vec3  fragNormal;
in vec2  fragLocalCoord;
flat in float fragFog;
flat in vec3  fragFogColor;

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

out vec4 finalColor;

const float PI = 3.141;

// ─── Helpers ────────────────────────────────────────────────────────────────

float luminance(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}

void main()
{
    if (spherical == 1) {
        // ── Disc → sphere mapping ───────────────────────────────────
        vec2 p = fragLocalCoord * 2.0;
        float r2 = dot(p, p);
        if (r2 > 1.0) discard;

        float z = sqrt(1.0 - r2);
        vec3 sphereNorm = vec3(p.x, p.y, z);

        // ── Camera basis ─────────────────────────────────────────────
        vec3 look     = normalize(cameraPos - billboardPos);
        vec3 camRight = normalize(cross(vec3(0.0, 1.0, 0.0), look));
        vec3 camUp    = cross(look, camRight);

        vec3 worldPt = sphereNorm.x * camRight + sphereNorm.y * camUp + sphereNorm.z * look;

        // Time-based Y rotation
        float angle = time * sphereSpeed;
        float ca = cos(angle), sa = sin(angle);
        vec3 rotPt = vec3(
            worldPt.x * ca + worldPt.z * sa,
            worldPt.y,
           -worldPt.x * sa + worldPt.z * ca
        );

        // ── Spherical UV ─────────────────────────────────────────────
        float u = atan(rotPt.x, rotPt.z) / (2.0 * PI) + 0.5;
        float v = asin(clamp(rotPt.y, -1.0, 1.0)) / PI + 0.5;
        vec2 sphereUV = vec2(u, v);

        vec4 texel = texture(texture0, sphereUV);
        vec3 albedo = texel.rgb * tintColor.rgb;

        // ── Roughness from inverted texture luminance ────────────────
        float texLum = luminance(texel.rgb);
        float rough  = clamp(1.0-texLum, 0.00, 1.0) * roughness;

        // ── World-space normal ───────────────────────────────────────
        // ── Sphere tangent basis (longitude/latitude) ──────────────
        vec3 N = normalize(sphereNorm.x * camRight + sphereNorm.y * camUp + sphereNorm.z * look);

        if (normalStrength > 0.0) {
            float eps = 0.005;
            float hc = luminance(texture(texture0, sphereUV).rgb);
            float hx = luminance(texture(texture0, sphereUV + vec2(eps, 0.0)).rgb);
            float hy = luminance(texture(texture0, sphereUV + vec2(0.0, eps)).rgb);

            float dhdx = (hx - hc) / eps;
            float dhdy = (hy - hc) / eps;

            // Tangent/bitangent aligned to UV on the sphere surface
            vec3 T = normalize(cross(vec3(0.0, 1.0, 0.0), N));
            vec3 B = cross(N, T);

            N = normalize(N - normalStrength * (dhdx * T + dhdy * B));
        }

        // ── Lighting ────────────────────────────────────────────────
        vec3 L    = normalize(-sunDir);
        vec3 V    = normalize(cameraPos - billboardPos);
        vec3 H    = normalize(L + V);

        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        float diff  = NdotL * 0.5 + 0.5;            // half-lambert for diffuse wrap
        float mask  = smoothstep(0.0, 1.0, diff);    // removed the double-square

        float r22   = rough * rough;
        float spec = pow(NdotH, 2.0 / max(r22, 0.001));  // stronger exponent
        vec3 specC = mix(vec3(0.04), albedo, metallic);

        vec3 ambient = albedo * ambientColor * 0.2;
        vec3 sunlit  = albedo * sunColor * 0.2 * NdotL
                    + specC * sunColor * spec * 1.5;     // was 0.3, now visible
        vec3 color   = ambient + sunlit;

        // ── Bottom darken ────────────────────────────────────────────
        color = mix(color, vec3(0.02), smoothstep(0.0, -1.0, p.y));

        // ── Fog ──────────────────────────────────────────────────────
        color = mix(color, fragFogColor, fragFog);

        finalColor = vec4(color, 0.0);

    } else {
        // ── Original flat billboard path ────────────────────────────
        vec4 texel = texture(texture0, fragTexCoord);
        if (texel.a < alphaThreshold) discard;

        vec3 albedo = texel.rgb * tintColor.rgb;

        vec3 N = normalize(fragNormal);
        if (normalStrength > 0.0) {
            float lum  = dot(texel.rgb, vec3(0.299, 0.587, 0.114));
            float dldx = dFdx(lum);
            float dldy = dFdy(lum);
            vec3  T    = normalize(dFdx(fragWorldPos));
            vec3  B    = normalize(dFdy(fragWorldPos));
            N = normalize(N + normalStrength * (-dldx * T - dldy * B));
        }

        vec3  L    = normalize(-sunDir);
        float diff = dot(N, L) * 0.5 + 0.5;
        float mask = smoothstep(0.0, 1.0, diff * diff);

        vec3 ambient = albedo * (ambientColor * 0.1);
        vec3 sunlit  = albedo * sunColor * 0.4;
        vec3 color   = mix(ambient, sunlit, mask);

        float rayDist   = distance(cameraPos, billboardPos);
        float exponent  = fragFog * max(rayDist, 0.0);
        float fogFactor = 1.0 - exp(-exponent * exponent);
        color = mix(color, fragFogColor, fogFactor);

        finalColor = vec4(color, 0.0);
    }
}
