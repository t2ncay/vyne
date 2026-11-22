#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D inputTexture;
uniform sampler2D inputTexture1;
uniform float strength;
uniform vec3 bloomTint;
void main() {
    vec4 source = texture(inputTexture,fragTexCoord);
    vec3 bloom = texture(inputTexture1,fragTexCoord).rgb;
    finalColor = vec4(source.rgb+bloom*bloomTint*strength,source.a);
}
