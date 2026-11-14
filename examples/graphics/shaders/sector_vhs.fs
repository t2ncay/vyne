#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform float time;
uniform vec2 renderSize;
uniform float noise_amount;

vec3 rgb2yiq(vec3 c) {
    return vec3(dot(c, vec3(0.2989, 0.5870, 0.1140)),
                dot(c, vec3(0.5959, -0.2744, -0.3216)),
                dot(c, vec3(0.2115, -0.5229, 0.3114)));
}
vec3 yiq2rgb(vec3 c) {
    return vec3(c.x + 0.956 * c.y + 0.6210 * c.z,
                c.x - 0.2720 * c.y - 0.6474 * c.z,
                c.x - 1.1060 * c.y + 1.7046 * c.z);
}
vec2 blurOffset(float point) {
    float angle = 6.2831853 * (point + 2.0 / 14.0) / 14.0;
    return vec2(-(0.3 + angle), cos(angle));
}
vec3 blur(vec2 uv, float amount) {
    vec3 color = texture(texture0, uv).rgb / 15.0;
    for (int i = 0; i < 14; ++i) color += texture(texture0, uv + blurOffset(float(i)) * vec2(amount * 5.28, 0.0)).rgb / 15.0;
    return color;
}
float randomNoise(vec2 pixel) {
    return fract(sin(dot(pixel, vec2(12.9898, 78.233))) * 43758.5453);
}
void main() {
    float phase = mod(time, 7.0);
    float distortion = clamp(noise_amount, 0.0, 1.0);
    vec2 uv = fragTexCoord;
    float window = 1.0 / (1.0 + 80.0 * pow(uv.y - mod(phase / 4.0, 1.0), 2.0));
    uv.x += 0.0025 * sin(uv.y * 10.0 + phase) * (0.5 + cos(phase * 20.0)) * window;
    uv.x += distortion * 0.025 * sin(uv.y * 80.0 + time * 40.0);
    float smear = max(0.0001, 0.002 * (0.051 + abs(sin(phase * 0.25)) * 0.0001 + distortion * 0.1));
    float luminance = rgb2yiq(blur(uv, smear + smear * uv.x)).r;
    uv.x += 0.00051 + distortion * 0.003;
    float chromaI = rgb2yiq(blur(uv, smear * 6.0)).g;
    uv.x += 0.000255 + distortion * 0.002;
    float chromaQ = rgb2yiq(blur(uv, smear * 2.5)).b;
    vec3 color = yiq2rgb(vec3(luminance, chromaI, chromaQ));
    vec2 pixel = floor(fragTexCoord * max(renderSize, vec2(1.0)));
    color += (randomNoise(pixel + floor(time * 24.0)) - 0.5) * distortion * 0.4;
    vec2 centered = fragTexCoord - vec2(0.5);
    color *= clamp(1.0 - dot(centered, centered) * 1.8, 0.0, 1.0);
    finalColor = vec4(color, 1.0);
}
