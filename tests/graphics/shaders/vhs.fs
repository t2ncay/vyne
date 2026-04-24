#version 330

// --- INPUTS ---
in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

// --- UNIFORMS ---
uniform sampler2D texture0;
uniform float time;
uniform vec2 renderSize;
uniform float offset;

// --- CONSTANTS/PARAMETERS ---
float wiggle = 0.0;
float smear = 1.0;

vec3 rgb2yiq(vec3 c) {
    return vec3(
        (0.2989*c.x + 0.5870*c.y + 0.1140*c.z),
        (0.5959*c.x - 0.2744*c.y - 0.3216*c.z),
        (0.2115*c.x - 0.5229*c.y + 0.3114*c.z)
    );
}

vec3 yiq2rgb(vec3 c) {
    return vec3(
        ( 1.0*c.x + 0.956*c.y + 0.6210*c.z),
        ( 1.0*c.x - 0.2720*c.y - 0.6474*c.z),
        ( 1.0*c.x - 1.1060*c.y + 1.7046*c.z)
    );
}

vec2 Circle(float Start, float Points, float Point) {
    float Rad = (3.141592 * 2.0 * (1.0 / Points)) * (Point + Start);
    return vec2(-(.3+Rad), cos(Rad));
}

vec3 Blur(vec2 uv, float d, float iTime) {
    float b = 1.0;
    vec2 PixelOffset = vec2(d, 0.0);
    float Start = 2.0 / 14.0;
    vec2 Scale = 0.66 * 4.0 * 2.0 * PixelOffset.xy;

    vec3 blurCol = vec3(0.0);
    float W = 1.0 / 15.0;

    for (int i = 0; i < 14; i++) {
        blurCol += texture(texture0, uv + Circle(Start, 14.0, float(i)) * Scale).rgb * W;
    }
    blurCol += texture(texture0, uv).rgb * W;

    return blurCol * b;
}

float onOff(float a, float b, float c, float frame) {
    return step(c, sin((frame * 0.001) + a * cos((frame * 0.001) * b)));
}

vec2 jumpy(vec2 uv, float frame) {
    vec2 look = uv;
    float window = 1.0 / (1.0 + 80.0 * (look.y - mod(frame/4.0, 1.0)) * (look.y - mod(frame/4.0, 1.0)));
    look.x += 0.05 * sin(look.y * 10.0 + frame) / 20.0 * onOff(4.0, 4.0, 0.3, frame) * (0.5 + cos(frame * 20.0)) * window;
    
    float vShift = (0.1 * (wiggle + offset * 5.0)) * 0.4 * onOff(2.0, 3.0, 0.9, frame) * (sin(frame) * sin(frame * 20.0) + (0.5 + 0.1 * sin(frame * 200.0) * cos(frame)));
    look.y = mod(look.y - 0.01 * vShift, 1.0);
    return look;
}

void main() {
    float iTime = mod(time, 7.0);
    float d_val = 0.1 - ceil(mod(iTime/3.0, 1.0) + 0.5) * 0.1;
    
    vec2 uv = jumpy(fragTexCoord, iTime);
    
    float s = 0.0001 * -d_val + 0.0001 * (wiggle + offset * 10.0) * sin(iTime);
    float e = min(0.30, pow(max(0.0, cos(uv.y * 4.0 + 0.3) - 0.75) * (s + 0.5) * 1.0, 3.0)) * 25.0;
    
    float final_d = 0.051 + abs(sin(s / 4.0)) + (offset * 0.5);
    float c_val = max(0.0001, 0.002 * final_d) * smear;
    
    float y = rgb2yiq(Blur(uv, c_val + c_val * uv.x, iTime)).r;
    
    uv.x += 0.01 * final_d + (offset * 0.02);
    float i = rgb2yiq(Blur(uv, c_val * 6.0, iTime)).g;
    
    uv.x += 0.005 * final_d + (offset * 0.01);
    float q = rgb2yiq(Blur(uv, c_val * 2.50, iTime)).b;

    y += (offset * 0.4) * (0.8 + 0.2 * sin(time * 50.0)); 

    i *= (1.0 + offset * 3.0);
    q *= (1.0 + offset * 3.0);

    vec3 finalRGB = yiq2rgb(vec3(y, i, q));

    finalRGB += pow(offset, 2.0) * 0.5;

    vec2 dist = fragTexCoord - vec2(0.5);
    float vignette = 1.0 - dot(dist, dist) * 1.8;
    finalRGB *= clamp(vignette, 0.0, 1.0);

    finalColor = vec4(finalRGB, 1.0);
}