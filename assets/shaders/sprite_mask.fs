#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;  // sprite atlas
uniform sampler2D texture1;  // mask texture (optional, bind if needed)
uniform float     alphaThreshold;  // discard below this (e.g. 0.1)
uniform vec4      tintColor;       // custom tint overlay

out vec4 finalColor;

void main()
{
    vec4 texel = texture(texture0, fragTexCoord);

    // Alpha cutout
    if (texel.a < alphaThreshold) discard;

    // Optional mask: multiply alpha by mask's red channel
    // vec4 mask = texture(texture1, fragTexCoord);
    // texel.a *= mask.r;
    // if (texel.a < alphaThreshold) discard;

    finalColor = texel * fragColor * tintColor;
}