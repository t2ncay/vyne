#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragPosition;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 cameraPos;
void main() {
    float dist = distance(fragPosition, cameraPos);
    float fogFactor = clamp(1.0 / exp(dist * 0.025), 0.0, 1.0);
    vec4 texelColor = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    finalColor = mix(vec4(0.5, 0.5, 0.55, 1.0), texelColor, fogFactor);
}
