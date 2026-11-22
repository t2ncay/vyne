#version 330
// Original bodycam's YIQ chroma smear, line jitter and vignette. Only time is
// configured by the application; offset defaults to zero. No unused renderSize.
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform float time;
uniform float offset;

vec3 rgb2yiq(vec3 c) {
    return vec3(dot(c, vec3(0.2989, 0.5870, 0.1140)),
                dot(c, vec3(0.5959, -0.2744, -0.3216)),
                dot(c, vec3(0.2115, -0.5229, 0.3114)));
}
vec3 yiq2rgb(vec3 c) {
    return vec3(c.x + 0.956*c.y + 0.6210*c.z,
                c.x - 0.2720*c.y - 0.6474*c.z,
                c.x - 1.1060*c.y + 1.7046*c.z);
}
vec2 circle(float start, float points, float point) {
    float rad = 3.141592 * 2.0 / points * (point + start);
    return vec2(-(.3 + rad), cos(rad));
}
vec3 blur(vec2 uv, float d) {
    vec2 scale = 0.66 * 4.0 * 2.0 * vec2(d, 0.0);
    vec3 color = vec3(0.0);
    for (int i = 0; i < 14; i++) {
        color += texture(texture0, uv + circle(2.0 / 14.0, 14.0, float(i)) * scale).rgb / 15.0;
    }
    return color + texture(texture0, uv).rgb / 15.0;
}
float onOff(float a, float b, float c, float frame) {
    return step(c, sin(frame * 0.001 + a * cos(frame * 0.001 * b)));
}
vec2 jumpy(vec2 uv, float frame) {
    vec2 look = uv;
    float delta = look.y - mod(frame / 4.0, 1.0);
    float window = 1.0 / (1.0 + 80.0 * delta * delta);
    look.x += 0.05 * sin(look.y * 10.0 + frame) / 20.0 * onOff(4.0, 4.0, 0.3, frame) * (0.5 + cos(frame * 20.0)) * window;
    float shift = offset * 0.2 * onOff(2.0, 3.0, 0.9, frame) * (sin(frame) * sin(frame * 20.0) + 0.5 + 0.1 * sin(frame * 200.0) * cos(frame));
    look.y = mod(look.y - 0.01 * shift, 1.0);
    return look;
}
void main() {
    float t = mod(time, 7.0);
    float d = 0.1 - ceil(mod(t / 3.0, 1.0) + 0.5) * 0.1;
    vec2 uv = jumpy(fragTexCoord, t);
    float s = -0.0001 * d + 0.001 * offset * sin(t);
    float finalD = 0.051 + abs(sin(s / 4.0)) + offset * 0.5;
    float smear = max(0.0001, 0.002 * finalD);
    float y = rgb2yiq(blur(uv, smear + smear * uv.x)).r;
    uv.x += 0.01 * finalD + offset * 0.02;
    float i = rgb2yiq(blur(uv, smear * 6.0)).g;
    uv.x += 0.005 * finalD + offset * 0.01;
    float q = rgb2yiq(blur(uv, smear * 2.50)).b;
    y += offset * 0.4 * (0.8 + 0.2 * sin(time * 50.0));
    vec3 color = yiq2rgb(vec3(y, i * (1.0 + offset * 3.0), q * (1.0 + offset * 3.0)));
    color += offset * offset * 0.5;
    vec2 dist = fragTexCoord - vec2(0.5);
    finalColor = vec4(color * clamp(1.0 - dot(dist, dist) * 1.8, 0.0, 1.0), 1.0);
}
