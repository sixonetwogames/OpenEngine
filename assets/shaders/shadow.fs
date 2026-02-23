#version 330

in vec2 fragUV;

uniform float opacity;
uniform vec3  color;

out vec4 finalColor;

void main() {
    vec2 centered = fragUV * 2.0 - 1.0;
    float dist = length(centered);

    float alpha = opacity * smoothstep(0.75, 0.15, dist);
    if (alpha < 0.01) discard;

    finalColor = vec4(color, alpha);
}