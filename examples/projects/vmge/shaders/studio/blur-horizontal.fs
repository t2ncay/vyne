#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D inputTexture;
uniform float texelStep;
uniform float radius;
void main() {
    vec2 stepUV = vec2(texelStep*radius,0.0);
    finalColor = texture(inputTexture,fragTexCoord-2.0*stepUV)*0.0625
               + texture(inputTexture,fragTexCoord-stepUV)*0.25
               + texture(inputTexture,fragTexCoord)*0.375
               + texture(inputTexture,fragTexCoord+stepUV)*0.25
               + texture(inputTexture,fragTexCoord+2.0*stepUV)*0.0625;
}
