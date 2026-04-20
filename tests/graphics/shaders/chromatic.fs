#version 330

in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;
uniform float time;

void main() {
    vec2 uv = fragTexCoord;
    
    float amount = 0.001 + 0.001 * sin(time * 1.0);
    
    float r = texture(texture0, uv + vec2(amount, 0.0)).r;
    float g = texture(texture0, uv).g;
    float b = texture(texture0, uv - vec2(amount, 0.0)).b;
    
    finalColor = vec4(r, g, b, 1.0);
}