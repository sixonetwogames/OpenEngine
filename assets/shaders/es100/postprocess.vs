#version 100
precision mediump float;
precision mediump int;

attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;

varying vec2 fragUV;

uniform mat4 mvp;

void main() {
    fragUV = vertexTexCoord;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}