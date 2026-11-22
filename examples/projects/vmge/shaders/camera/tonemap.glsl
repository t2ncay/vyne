in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D inputTexture;
uniform float exposure;
void main() {
    vec4 source = texture(inputTexture, fragTexCoord);
    vec3 color = max(source.rgb * exposure, vec3(0.0));
    color = color / (vec3(1.0) + color);
    finalColor = vec4(pow(color, vec3(1.0 / 2.2)), source.a);
}
