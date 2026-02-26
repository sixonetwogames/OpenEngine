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
uniform float fogNear;
uniform float fogFar;
uniform vec3  fogColor;
uniform vec3  cameraPos;

varying vec2  fragTexCoord;
varying vec3  fragWorldPos;
varying vec3  fragNormal;
varying float fragFog;
varying vec3  fragFogColor;

uniform float time;
uniform float wind;    // amplitude of sway
uniform vec3  windDirection;   // XY = normalized dir, Z = frequency
uniform int   windEnabled;     // toggle wind per-billboard

void main()
{
    vec3 camRight, camUp;

    if (lockY == 1) {
        vec3 look = normalize(vec3(cameraPos.x - billboardPos.x, 0.0, cameraPos.z - billboardPos.z));
        camRight = vec3(-look.z, 0.0, look.x);
        camUp    = vec3(0.0, 1.0, 0.0);
    } else {
        vec3 look = normalize(cameraPos - billboardPos);
        camRight  = normalize(cross(vec3(0.0, 1.0, 0.0), look));
        camUp     = cross(look, camRight);
    }

    // Normalized height: 0 at base, 1 at top
    float heightFactor = vertexPosition.z + 0.5;

    // Origin at base, offset up by half height
    vec3 worldPos = billboardPos
                  + camRight * vertexPosition.x * billboardSize.x
                  + camUp    * heightFactor * billboardSize.y;

    // Wind sway — scaled by height² so base stays planted, top sways most
    if (windEnabled != 0) {
        float freq      = windDirection.z;
        // Position-based phase offset for per-tree variation
        float phase     = dot(billboardPos, vec3(7.31, 0.0, 13.57));
        float sway      = sin(time * freq + phase) * wind * 1.0* heightFactor * heightFactor;
        worldPos.xz    += windDirection.xy * sway;
    }

    vec4 viewPos = matView * vec4(worldPos, 1.0);
    gl_Position  = matProjection * viewPos;

    // Fog from billboard center distance (uniform across whole sprite)
    float centerDepth = -(matView * vec4(billboardPos, 1.0)).z;
    fragFog      = clamp((centerDepth - fogNear) / (fogFar - fogNear), 0.0, 1.0);
    fragFogColor = fogColor;

    fragTexCoord = mix(uvMin, uvMax, vec2(vertexTexCoord.x, 1.0 - vertexTexCoord.y));
    fragWorldPos = worldPos;
    fragNormal   = normalize(cameraPos - billboardPos);
}
