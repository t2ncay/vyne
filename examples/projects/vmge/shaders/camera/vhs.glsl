in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D inputTexture;
uniform float seTime;
uniform vec2 seInputResolution;
uniform float strength, grain, scanlines, wobble, separation, vignette;
float noise(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
void main() {
    vec2 uv = fragTexCoord;
    vec4 original = texture(inputTexture, uv);
    float tracking = sin(uv.y * 75.0 + seTime * 7.0) * wobble / seInputResolution.x;
    uv.x = clamp(uv.x + tracking, 0.0, 1.0);
    vec2 offset = vec2(separation / seInputResolution.x, 0.0);
    vec3 color = vec3(texture(inputTexture, clamp(uv + offset, 0.0, 1.0)).r,
                      texture(inputTexture, uv).g,
                      texture(inputTexture, clamp(uv - offset, 0.0, 1.0)).b);
    float line = 0.5 + 0.5 * cos(uv.y * seInputResolution.y * 3.14159265);
    color *= 1.0 - scanlines * line;
    color += (noise(floor(uv * seInputResolution) + floor(seTime * 30.0)) - 0.5) * grain;
    vec2 center = uv * 2.0 - 1.0;
    color *= 1.0 - vignette * clamp(dot(center, center) * 0.5, 0.0, 1.0);
    finalColor = vec4(mix(original.rgb, max(color, vec3(0.0)), strength), original.a);
}
