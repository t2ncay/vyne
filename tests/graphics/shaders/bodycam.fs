#version 330
in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;
uniform float time;

void main() {
    vec2 uv = fragTexCoord;
    vec2 center = vec2(0.5, 0.5);
    vec2 dist = uv - center;
    float r = length(dist);

    float zoom = 1.0 + 0.05 * r * r; 
    vec2 targetUV = center + dist * zoom;

    targetUV = clamp(targetUV, 0.001, 0.999);

    vec4 texColor = texture(texture0, targetUV);
    
    float edgeMask = smoothstep(0.5, 0.48, abs(targetUV.x - 0.5)) * smoothstep(0.5, 0.48, abs(targetUV.y - 0.5));

    float v = smoothstep(1.3, 0.6, r);

    finalColor = vec4(texColor.rgb * edgeMask * (v + 0.15), 1.0);
}