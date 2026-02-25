#version 100
precision mediump float;

attribute vec3 vertexPosition;

uniform mat4 mvp;
uniform mat4 matModel;

varying vec3 fragWorldPos;

void main()
{
    fragWorldPos = vec3(matModel * vec4(vertexPosition, 1.0));
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}