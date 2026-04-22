#version 330

in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;
uniform float time;
uniform vec2 renderSize;
uniform float isEnabled;

void main() {
    vec2 uv = fragTexCoord;
    vec4 texel = texture(texture0, uv);
    
    if (isEnabled < 0.5) {
        finalColor = texel;
        return;
    }

    vec2 center = vec2(0.5);
    float dist = distance(uv, center);

    float flashlight = smoothstep(0.45, 0.05, dist); 
    
    float hotspot = exp(-dist * 10.0) * 2.5;

    vec3 lightEffect = texel.rgb * (flashlight * 3.5 + hotspot + 0.15);
    
    vec3 finalRGB = lightEffect * vec3(1.0, 0.98, 0.9); 

    // 5. Titrəmə (Flicker) - Çox incə bir titrəmə
    float flicker = 1.0 + sin(time * 25.0) * 0.02;

    finalColor = vec4(finalRGB * flicker, 1.0);
}