#version 330

// --- INPUTS ---
in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

// --- UNIFORMS ---
uniform sampler2D texture0; // Raylib default
uniform float time;         // vglib-dən göndərdiyin run_time
uniform vec2 renderSize;    // Ekran ölçüsü

// --- PARAMETERS ---
float magnitude = 0.9;
float always_on = 0.0;

float rand(vec2 co) {
    return fract(sin(dot(co.xy ,vec2(12.9898, 78.233))) * 43758.5453);
}

vec4 hash42(vec2 p) {
    vec4 p4 = fract(vec4(p.xyxy) * vec4(443.8975, 397.2973, 491.1871, 470.7827));
    p4 += dot(p4.wzxy, p4 + 19.19);
    return fract(vec4(p4.x * p4.y, p4.x * p4.z, p4.y * p4.w, p4.x * p4.w));
}

float hash(float n) {
    return fract(sin(n) * 43758.5453123);
}

float n(vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    float n = p.x + p.y * 57.0 + 113.0 * p.z;
    return mix(mix(mix(hash(n + 0.0), hash(n + 1.0), f.x),
                   mix(hash(n + 57.0), hash(n + 58.0), f.x), f.y),
               mix(mix(hash(n + 113.0), hash(n + 114.0), f.x),
                   mix(hash(n + 170.0), hash(n + 171.0), f.x), f.y), f.z);
}

float nn(vec2 p, float timer) {
    float y = p.y;
    float s = mod(timer * 0.15, 4837.0);
    float v = n(vec3(y * 0.01 + s, 1.0, 1.0)) * n(vec3(y * 0.011 + 1000.0 + s, 1.0, 1.0)) * n(vec3(y * 0.51 + 421.0 + s, 1.0, 1.0));
    v *= hash42(vec2(p.x + timer * 0.01, p.y)).x + 0.3;
    v = pow(v + 0.3, 1.0);
    return (v < 0.99) ? 0.0 : v;
}

float onOff(float a, float b, float c, float timer) {
    return step(c, sin((timer * 0.001) + a * cos((timer * 0.001) * b)));
}

vec2 jumpy(vec2 uv, float timer) {
    vec2 look = uv;
    float window = 1.0 / (1.0 + 80.0 * (look.y - mod(timer / 4.0, 1.0)) * (look.y - mod(timer / 4.0, 1.0)));
    look.x += 0.05 * sin(look.y * 10.0 + timer) / 20.0 * onOff(4.0, 4.0, 0.3, timer) * (0.5 + cos(timer * 20.0)) * window;
    float vShift = 0.4 * onOff(2.0, 3.0, 0.9, timer) * (sin(timer) * sin(timer * 20.0) + (0.5 + 0.1 * sin(timer * 200.0) * cos(timer)));
    look.y = mod(look.y - 0.01 * vShift, 1.0);
    return look;
}

void main() {
    float timer = time * 60.0; // RetroArch FrameCount simulyasiyası
    vec2 uv = jumpy(fragTexCoord, timer);
    
    float mag = magnitude * 0.0001;
    vec2 offset_x = vec2(uv.x);
    offset_x.x += rand(vec2(mod(timer, 9847.0) * 0.03, uv.y * 0.42)) * 0.001 + sin(rand(vec2(mod(timer, 5583.0) * 0.2, uv.y))) * mag;
    
    vec3 res = vec3(
        texture(texture0, vec2(offset_x.x, uv.y)).r,
        texture(texture0, vec2(offset_x.x, uv.y)).g,
        texture(texture0, uv).b
    );
    
    // Tape Noise (Ağ horizontal xəttlər/parazitlər)
    float col = nn(-fragTexCoord * renderSize.y * 4.0, timer);
    
    finalColor = vec4(res + clamp(vec3(col), 0.0, 0.5), 1.0);
}