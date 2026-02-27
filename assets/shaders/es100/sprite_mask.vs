#version 100
precision mediump float;
precision mediump int;

attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;

uniform mat4 matView;
uniform mat4 matProjection;

uniform vec3  billboardPos;
uniform vec2  billboardSize;
uniform vec2  uvMin;
uniform vec2  uvMax;
uniform int   lockY;
uniform int   spherical;
uniform float fogNear;
uniform float fogFar;
uniform vec3  fogColor;
uniform vec3  cameraPos;

uniform float time;
uniform float wind;
uniform vec3  windDirection;
uniform int   windEnabled;

varying vec2  fragTexCoord;
varying vec3  fragWorldPos;
varying vec3  fragNormal;
varying vec2  fragLocalCoord;
varying float fragFog;
varying vec3  fragFogColor;

void main()
{
    vec3 look, camRight, camUp;

    if (spherical == 1) {
        // ── Full camera-facing (all 3 axes) ─────────────────────────
        look     = normalize(cameraPos - billboardPos);
        camRight = normalize(cross(vec3(0.0, 1.0, 0.0), look));
        camUp    = cross(look, camRight);

        // Center-anchored (not base-anchored like trees)
        vec3 worldPos = billboardPos
                      + camRight * vertexPosition.x * billboardSize.x
                      + camUp    * vertexPosition.z * billboardSize.y;

        fragWorldPos   = worldPos;
        fragLocalCoord = vec2(vertexPosition.x, vertexPosition.z); // -0.5..0.5

        vec4 viewPos = matView * vec4(worldPos, 1.0);
        gl_Position  = matProjection * viewPos;

    } else {
        // ── Existing billboard logic ────────────────────────────────
        if (lockY == 1) {
            look     = normalize(vec3(cameraPos.x - billboardPos.x, 0.0, cameraPos.z - billboardPos.z));
            camRight = vec3(-look.z, 0.0, look.x);
            camUp    = vec3(0.0, 1.0, 0.0);
        } else {
            look     = normalize(cameraPos - billboardPos);
            camRight = normalize(cross(vec3(0.0, 1.0, 0.0), look));
            camUp    = cross(look, camRight);
        }

        float heightFactor = vertexPosition.z + 0.5;
        vec3 worldPos = billboardPos
                      + camRight * vertexPosition.x * billboardSize.x
                      + camUp    * heightFactor * billboardSize.y;

        if (windEnabled != 0) {
            float freq  = windDirection.z;
            float phase = dot(billboardPos, vec3(7.31, 0.0, 13.57));
            float sway  = sin(time * freq + phase) * wind * 1.0 * heightFactor * heightFactor;
            vec2 windOff = windDirection.xy * sway;
            worldPos.x += windOff.x;
            worldPos.z += windOff.y;
        }

        fragWorldPos   = worldPos;
        fragLocalCoord = vec2(0.0);

        vec4 viewPos = matView * vec4(worldPos, 1.0);
        gl_Position  = matProjection * viewPos;
    }

    // Fog from billboard center
    float centerDepth = -(matView * vec4(billboardPos, 1.0)).z;
    fragFog      = clamp((centerDepth - fogNear) / (fogFar - fogNear), 0.0, 1.0);
    fragFogColor = fogColor;

    fragTexCoord = mix(uvMin, uvMax, vec2(vertexTexCoord.x, 1.0 - vertexTexCoord.y));
    fragNormal   = normalize(cameraPos - billboardPos);
}
