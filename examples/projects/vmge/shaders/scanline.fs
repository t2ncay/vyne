#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 renderSize;
const float PI = 3.141592653589;
void main() {
    vec2 uv = fragTexCoord;
    vec3 color = texture(texture0, uv).rgb;
    float scanline = mix(1.0, sin(uv.y * renderSize.y * PI) * 0.5 + 0.5, 0.5);
    float mask = mix(1.0, sin(uv.x * renderSize.x * PI) * 0.5 + 0.5, 0.15);
    finalColor = vec4(color * scanline * mask * 1.1, 1.0);
}
