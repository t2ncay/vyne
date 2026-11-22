#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec3 studioPosition;
out vec3 studioLocal;
out vec3 studioNormal;
void main() {
    studioPosition = (matModel*vec4(vertexPosition,1.0)).xyz;
    studioLocal = vertexPosition;
    studioNormal = (matNormal*vec4(vertexNormal,0.0)).xyz;
    gl_Position = mvp*vec4(vertexPosition,1.0);
}
