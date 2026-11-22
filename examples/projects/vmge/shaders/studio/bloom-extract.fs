#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D inputTexture;
uniform float threshold;
uniform float knee;
uniform float gain;
void main() {
    vec4 source = texture(inputTexture,fragTexCoord);
    float peak = max(source.r,max(source.g,source.b));
    float gate = clamp((peak-threshold)/knee,0.0,1.0);
    finalColor = vec4(max(source.rgb-vec3(threshold),vec3(0.0))*gate*gain,source.a);
}
