#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D inputTexture;
uniform float gain;
#include "common.glsl"
void main() { finalColor=vec4(studio_gain(texture(inputTexture,fragTexCoord).rgb,gain),1.0); }
