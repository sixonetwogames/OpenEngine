#version 100
precision mediump float;

varying vec2 fragTexCoord;
varying vec4 fragColor;

uniform sampler2D texture0;  // sprite atlas
uniform sampler2D texture1;  // mask texture2D(optional, bind if needed)
uniform float     alphaThreshold;  // discard below this (e.g. 0.1)
uniform vec4      tintColor;       // custom tint overlay


void main()
{
    vec4 texel = texture2D(texture0, fragTexCoord);

    // Alpha cutout
    if (texel.a < alphaThreshold) discard;

    // Optional mask: multiply alpha by mask's red channel
    // vec4 mask = texture2D(texture1, fragTexCoord);
    // texel.a *= mask.r;
    // if (texel.a < alphaThreshold) discard;

    gl_FragColor = texel * fragColor * tintColor;
}