#version 330

in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;
uniform float time;

float noise(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 uv = fragTexCoord;
    
    float amount = 0.002 * sin(time * 2.0);
    float r = texture(texture0, uv + vec2(amount, 0.0)).r;
    float g = texture(texture0, uv).g;
    float b = texture(texture0, uv - vec2(amount, 0.0)).b;
    
    vec3 color = vec3(r, g, b);
    
    float scanline = sin(uv.y * 800.0) * 0.02;
    color -= scanline;
    
    float n = (noise(uv + time) - 0.5) * 0.1;
    color += n;
    
    float dist = distance(uv, vec2(0.5, 0.5));
    color *= smoothstep(0.8, 0.2, dist);
    
    finalColor = vec4(color, 1.0);
}