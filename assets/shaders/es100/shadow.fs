#version 100
precision mediump float;
precision mediump int;

varying vec2 fragUV;

uniform float opacity;
uniform vec3  color;


void main() {
    float dist  = length(fragUV * 2.0 - 1.0);
    float alpha = opacity * smoothstep(0.75, 0.15, dist);
    gl_FragColor  = vec4(color, alpha);
}