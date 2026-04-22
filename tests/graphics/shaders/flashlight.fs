#version 330

in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;
uniform float time;

void main() {
    vec2 uv = fragTexCoord;
    vec4 texel = texture(texture0, uv);
    
    float dist = distance(uv, vec2(0.5));
    
    float flashlight = smoothstep(0.5, 0.1, dist);
    
    vec3 scene = texel.rgb * (flashlight * 4.0 + 0.1);
    
    // Fənər titrəməsi
    float flicker = 1.0 + sin(time * 30.0) * 0.04;
    
    finalColor = vec4(scene * flicker, 1.0);
}