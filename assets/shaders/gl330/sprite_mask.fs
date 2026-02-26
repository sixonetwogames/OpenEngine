#version 330

in vec2  fragTexCoord;
in vec3  fragWorldPos;
in vec3  fragNormal;
flat in float fragFog;
flat in vec3  fragFogColor;

uniform sampler2D texture0;

uniform float alphaThreshold;
uniform vec4  tintColor;

uniform vec3  sunDir;
uniform vec3  sunColor;
uniform vec3  ambientColor;

uniform vec3 cameraPos;

uniform float roughness;
uniform float metallic;
uniform float normalStrength;

out vec4 finalColor;

void main()
{
    vec4 texel = texture(texture0, fragTexCoord);
    if (texel.a < alphaThreshold) discard;

    vec3  albedo = texel.rgb * tintColor.rgb;
    float alpha  = texel.a * tintColor.a;

    // ── Fake normal from luminance gradient ──────────────────────────────
    vec3 N = normalize(fragNormal);
    if (normalStrength > 0.0) {
        float lum  = dot(texel.rgb, vec3(0.299, 0.587, 0.114));
        float dldx = dFdx(lum);
        float dldy = dFdy(lum);
        vec3  T    = normalize(dFdx(fragWorldPos));
        vec3  B    = normalize(dFdy(fragWorldPos));
        N = normalize(N + normalStrength * (-dldx * T - dldy * B));
    }

    // ── Lighting ─────────────────────────────────────────────────────────
    vec3 L = normalize(-sunDir);

    // Half-lambert diffuse
    float NdotL = dot(N, L);
    float diff  = NdotL * 0.5 + 0.5;
    diff *= diff;

    // Blinn-Phong specular
    //vec3 V = normalize(cameraPos - fragWorldPos);
    //vec3  H     = normalize(L + V);
    //float NdotH = max(dot(N, H), 0.0);
    //float r4    = roughness * roughness * roughness * roughness;
    //float spec  = pow(NdotH, 2.0 / max(r4, 0.001) - 2.0);
    //vec3  specC  = mix(vec3(0.04), albedo, metallic);
    vec3 color = mix (albedo, ambientColor, diff);
    color = albedo * (sunColor * diff*0.5);
    
               //+ specC * sunColor * spec;
    //color = mix(albedo, )

    // ── Fog (uniform across sprite, blend toward fog color) ─────────────
    color = mix(color, fragFogColor, fragFog);

    // Alpha=0 sentinel: tells post-process to skip fog for this pixel
    finalColor = vec4(color, 0.0);
}