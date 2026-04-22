#version 330

in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;
uniform float time;

void main() {
    vec2 uv = fragTexCoord;
    vec4 texel = texture(texture0, uv);
    
    // Mərkəzdən məsafə (0.5, 0.5 tam mərkəzdir)
    float dist = distance(uv, vec2(0.5));
    
    // Fənər dairəsi: radiusu bir az böyütdük (0.5) və daha parlaq etdik
    float flashlight = smoothstep(0.5, 0.1, dist);
    
    // İşığı rəsmi olaraq gücləndiririk (Boost)
    // 4.0 qatı dumanın (fog) içində hər şeyi göstərəcək
    vec3 scene = texel.rgb * (flashlight * 4.0 + 0.1);
    
    // Fənər titrəməsi
    float flicker = 1.0 + sin(time * 30.0) * 0.04;
    
    finalColor = vec4(scene * flicker, 1.0);
}