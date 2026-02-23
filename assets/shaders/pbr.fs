#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;
in mat3 fragTBN;

uniform sampler2D texture0; // albedo
uniform sampler2D texture1; // normal
uniform sampler2D texture2; // roughness / metalness

uniform vec3  viewPos;
uniform vec3  albedoColor;
uniform float metallic;
uniform float roughness;
uniform float ambientStrength;
uniform vec3  ambientColor;

// Normal mapping
uniform int   useNormalMap;
uniform float normalStrength;

// Post adjustments
uniform float brightness;
uniform float contrast;

// UV control
uniform int   useWorldUVs;
uniform float tiling;

// Roughness map
uniform int   useRoughnessMap;

// Fog
uniform float fogNear;
uniform float fogFar;

// Lighting
uniform vec3 lightDir;
uniform vec3 lightColor;

out vec4 finalColor;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float r) {
    float a  = r * r;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float r) {
    float k = (r + 1.0) * (r + 1.0) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float r) {
    return GeometrySchlickGGX(max(dot(N, V), 0.0), r)
         * GeometrySchlickGGX(max(dot(N, L), 0.0), r);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    // --- UV computation ---
    vec2 uv = (useWorldUVs > 0) ? fragPosition.xz * tiling : fragTexCoord * tiling;

    // --- Albedo ---
    vec4 albedo = texture(texture0, uv) * vec4(albedoColor, 1.0);

    // --- Roughness / Metallic ---
    float rough = roughness;
    float metal = metallic;
    if (useRoughnessMap > 0) {
        vec4 mrSample = texture(texture2, uv);
        rough *= mrSample.g;
        metal *= mrSample.b;
    }

    // --- Normal ---
    vec3 N;
    if (useNormalMap > 0) {
        vec3 normalMap = texture(texture1, uv).rgb * 2.0 - 1.0;
        normalMap.xy *= normalStrength;
        N = normalize(fragTBN * normalMap);
    } else {
        N = normalize(fragNormal);
    }

    // --- PBR lighting ---
    vec3 V  = normalize(viewPos - fragPosition);
    vec3 F0 = mix(vec3(0.04), albedo.rgb, metal);

    vec3 L = normalize(-lightDir);
    vec3 H = normalize(V + L);

    float NDF = DistributionGGX(N, H, rough);
    float G   = GeometrySmith(N, V, L, rough);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 spec = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
    vec3 kD   = (1.0 - F) * (1.0 - metal);

    float NdotL = max(dot(N, L), 0.0);
    vec3 Lo = (kD * albedo.rgb / PI + spec) * lightColor * NdotL;

    // --- Ambient / skylight ---
    float skyHemi = 0.5 + 0.5 * dot(N, vec3(0.0, 1.0, 0.0));
    vec3 ambient  = ambientColor * ambientStrength * albedo.rgb * skyHemi;

    vec3 color = ambient + Lo;

    // --- Brightness / Contrast ---
    color = (color - 0.5) * contrast + 0.5;
    color += brightness - 1.0;
    color = max(color, 0.0);

    // --- Fog depth packing ---
    float z = gl_FragCoord.z * 2.0 - 1.0;
    float linearDepth = (2.0 * fogNear * fogFar) / (fogFar + fogNear - z * (fogFar - fogNear));
    finalColor = vec4(color, linearDepth / fogFar);
}