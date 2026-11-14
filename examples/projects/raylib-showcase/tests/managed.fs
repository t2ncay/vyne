#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D sourceTex;
uniform vec4 tint;
uniform float factor;
uniform int selector;
uniform vec2 dimensions;
uniform vec3 shift;
uniform ivec3 counts;
void main() {
    vec4 color = texture(sourceTex, fragTexCoord);
    if (selector != 0) color = vec4(shift, 1.0);
    if (dimensions.x < 0.0 || dimensions.y < 0.0) color = vec4(0.0);
    if (counts.x + counts.y + counts.z != 0) color = vec4(0.0);
    finalColor = color * tint * factor;
}
