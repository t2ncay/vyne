#version 330
out vec4 finalColor;
uniform vec3 skyForward, skyRight, skyUp;
uniform vec2 skyResolution;
uniform float skyTanFov;
uniform vec3 skyZenith, skyHorizon, skySunDirection;
uniform float skyBrightness, skySunSize, skySunEnergy;
void main() {
    vec2 uv = gl_FragCoord.xy / skyResolution * 2.0 - 1.0;
    vec3 ray = normalize(skyForward + skyRight * uv.x * skyTanFov * skyResolution.x / skyResolution.y + skyUp * uv.y * skyTanFov);
    vec3 color = mix(skyHorizon, skyZenith, pow(clamp(ray.y, 0.0, 1.0), 0.65)) * skyBrightness;
    float disc = smoothstep(cos(skySunSize), cos(skySunSize * 0.8), dot(ray, normalize(skySunDirection)));
    finalColor = vec4(color + vec3(1.0, 0.88, 0.65) * disc * skySunEnergy, 1.0);
}
