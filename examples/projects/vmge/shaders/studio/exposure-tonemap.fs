#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D inputTexture;
uniform float exposure;
uniform float whitePoint;
uniform float saturation;
uniform float displayGamma;
void main() {
    vec4 source = texture(inputTexture,fragTexCoord);
    vec3 color = max(source.rgb,vec3(0.0))*exp2(exposure)/whitePoint;
    color = vec3(1.0)-exp(-color);
    float gray = dot(color,vec3(0.2126,0.7152,0.0722));
    color = max(mix(vec3(gray),color,saturation),vec3(0.0));
    finalColor = vec4(pow(color,vec3(1.0/displayGamma)),source.a);
}
