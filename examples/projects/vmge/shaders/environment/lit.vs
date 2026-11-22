#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp, matModel, matNormal;
uniform float seWorldMode;
out vec3 sePosition, seNormal;
out vec2 fragTexCoord;
out vec4 fragColor;
void main() {
    sePosition = seWorldMode > 0.5 ? vertexPosition : (matModel * vec4(vertexPosition, 1.0)).xyz;
    seNormal = (matNormal * vec4(vertexNormal, 0.0)).xyz;
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
