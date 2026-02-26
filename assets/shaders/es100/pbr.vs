#version 100
precision mediump float;
precision mediump int;

attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec3 vertexNormal;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

varying vec3 fragPosition;
varying vec2 fragTexCoord;
varying vec3 fragNormal;

void main()
{
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragNormal   = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    gl_Position  = mvp * vec4(vertexPosition, 1.0);
}