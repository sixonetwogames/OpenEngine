#version 330

in vec2 fragUV;

uniform float opacity;
uniform vec3  color;

out vec4 finalColor;

void main() {
    float dist  = length(fragUV * 2.0 - 1.0);
    float alpha = opacity * smoothstep(0.75, 0.15, dist);
    finalColor  = vec4(color, alpha);
}