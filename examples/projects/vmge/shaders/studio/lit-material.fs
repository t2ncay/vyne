#version 330
in vec3 studioPosition;
in vec3 studioLocal;
in vec3 studioNormal;
out vec4 finalColor;
uniform vec4 colDiffuse;
uniform vec3 baseColor;
uniform vec3 accentColor;
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform vec3 cameraPosition;
uniform float ambient;
uniform float contrast;
uniform float patternScale;
uniform float patternStrength;
uniform float fresnelStrength;
uniform float fresnelPower;
uniform float normalView;
vec3 safeDirection(vec3 v) { return v*inversesqrt(max(dot(v,v),1e-8)); }
void main() {
    vec3 n = safeDirection(studioNormal);
    vec3 l = safeDirection(lightDirection);
    vec3 v = safeDirection(cameraPosition-studioPosition);
    float stripe = 0.5+0.5*sin(6.28318530718*patternScale*dot(studioLocal,vec3(1.0,0.5,0.25)));
    vec3 albedo = mix(baseColor,accentColor,stripe*patternStrength);
    float diffuse = ambient+(1.0-ambient)*pow(max(dot(n,l),0.0),contrast);
    float rim = fresnelStrength*pow(1.0-clamp(dot(n,v),0.0,1.0),fresnelPower);
    vec3 color = albedo*lightColor*diffuse+lightColor*rim;
    color = mix(color,0.5*n+0.5,normalView);
    finalColor = vec4(color*colDiffuse.rgb,colDiffuse.a);
}
