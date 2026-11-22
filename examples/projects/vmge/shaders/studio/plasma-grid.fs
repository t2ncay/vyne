#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform vec3 lowColor;
uniform vec3 highColor;
uniform vec3 gridColor;
uniform float time;
uniform float period;
uniform float scale;
uniform float contrast;
uniform float gridCount;
uniform float lineWidth;
uniform float gridMix;
uniform float intensity;
void main() {
    float phase = 6.28318530718*mod(time,period)/period;
    vec2 p = (fragTexCoord-0.5)*scale;
    float wave = 0.5+0.25*(sin(6.28318530718*p.x+phase)+cos(6.28318530718*p.y-phase));
    float weight = clamp((wave-0.5)*contrast+0.5,0.0,1.0);
    vec3 color = mix(lowColor,highColor,weight);
    vec2 edge = abs(fract(fragTexCoord*gridCount-0.5)-0.5);
    float line = 1.0-smoothstep(lineWidth,lineWidth+0.015,min(edge.x,edge.y));
    finalColor = vec4(mix(color,gridColor,line*gridMix)*intensity,1.0);
}
