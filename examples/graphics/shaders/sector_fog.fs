#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragPosition;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 cameraPos;
uniform float u_density;
void main() {
    float distanceToCamera = distance(fragPosition, cameraPos);
    float visibility = clamp(exp(-distanceToCamera * max(u_density, 0.0)), 0.0, 1.0);
    vec4 fogColor = vec4(0.5, 0.5, 0.55, 1.0);
    vec4 surface = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    if (surface.a < 0.01) discard;
    finalColor = vec4(mix(fogColor.rgb, surface.rgb, visibility), surface.a);
}
