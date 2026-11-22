#version 330
in vec3 sePosition, seNormal;
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float seAmbient, seWorldMode;
uniform vec3 seSunDirection, seSunColor;
uniform vec3 seCameraPosition;
uniform int sePointCount;
uniform vec4 sePoint0, sePoint1, sePoint2, sePoint3;
uniform vec3 sePointColor0, sePointColor1, sePointColor2, sePointColor3;
vec3 pointContribution(vec4 light, vec3 color, vec3 normal) {
    vec3 delta = light.xyz - sePosition;
    float distance = length(delta);
    float attenuation = pow(clamp(1.0 - distance / max(light.w, 0.1), 0.0, 1.0), 2.0);
    return color * attenuation * max(dot(normal, delta / max(distance, 0.0001)), 0.0);
}
void main() {
    vec3 normal = normalize(seNormal);
    if (seWorldMode > 0.5) {
        normal = normalize(cross(dFdx(sePosition), dFdy(sePosition)));
        // Render-target projection can reverse screen-space winding. Orient
        // derivative normals toward the viewer rather than relying on GL's
        // front-face convention; immediate terrain/cubes are two-sided lit.
        if (dot(normal, seCameraPosition - sePosition) < 0.0) normal = -normal;
    }
    vec3 light = vec3(seAmbient) + seSunColor * max(dot(normal, normalize(seSunDirection)), 0.0);
    if (sePointCount > 0) light += pointContribution(sePoint0, sePointColor0, normal);
    if (sePointCount > 1) light += pointContribution(sePoint1, sePointColor1, normal);
    if (sePointCount > 2) light += pointContribution(sePoint2, sePointColor2, normal);
    if (sePointCount > 3) light += pointContribution(sePoint3, sePointColor3, normal);
    vec4 albedo = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    finalColor = vec4(albedo.rgb * light, albedo.a);
}
