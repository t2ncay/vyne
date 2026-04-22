#version 330

// --- INPUTS ---
in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

// --- UNIFORMS ---
uniform sampler2D texture0;
uniform vec2 renderSize; // Ekran ölçüsü (məs: 1920, 1080)

// --- PARAMETERS ---
// Bu dəyərlərlə oynayıb xəttlərin gücünü artıra bilərsən
float scanline_strength = 0.5; // SCANLINE_SINE_COMP_B
float grid_strength = 0.15;    // SCANLINE_SINE_COMP_A
float grid_size = 1.0;         // Piksellərin sıxlığı

#define PI 3.141592653589

void main()
{
    vec2 uv = fragTexCoord;
    vec3 res = texture(texture0, uv).xyz;

    float scanline = (sin(uv.y * renderSize.y * PI) * 0.5 + 0.5);
    
    scanline = mix(1.0, scanline, scanline_strength);

    float mask = (sin(uv.x * renderSize.x * PI / grid_size) * 0.5 + 0.5);
    mask = mix(1.0, mask, grid_strength);

    res *= (scanline * mask);
    
    res *= 1.1; 

    finalColor = vec4(res, 1.0);
}