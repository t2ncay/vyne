#version 330

// Input vertex attributes (from raylib)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniforms
uniform sampler2D texture0;
uniform vec2 textureSize;

// Output fragment color
out vec4 finalColor;

void main() {
    // Texel size (distance between low-res pixels in UV space)
    vec2 texel = vec2(1.0) / textureSize;

    // Sample the center pixel (already bilinearly upscaled by GPU hardware)
    vec4 center = texture(texture0, fragTexCoord);

    // Sample 4 immediate diagonal neighbors
    vec4 tl = texture(texture0, fragTexCoord + vec2(-texel.x, -texel.y));
    vec4 tr = texture(texture0, fragTexCoord + vec2( texel.x, -texel.y));
    vec4 bl = texture(texture0, fragTexCoord + vec2(-texel.x,  texel.y));
    vec4 br = texture(texture0, fragTexCoord + vec2( texel.x,  texel.y));

    // Find the min and max color intensity among neighbors to see if we are on an edge
    vec4 minColor = min(center, min(min(tl, tr), min(bl, br)));
    vec4 maxColor = max(center, max(max(tl, tr), max(bl, br)));

    // Contrast Adaptive math: Calculate contrast around this area
    vec4 contrast = maxColor - minColor;
    
    // Choose sharpening strength dynamically based on contrast.
    // If contrast is extremely high or low, don't over-sharpen to avoid artifacts.
    vec4 edgeWeight = -0.15 * (vec4(1.0) - contrast); 

    // Combine pixels: Center gets the most weight, neighbors subtract a bit to sharpen edges
    vec4 sharpened = center + edgeWeight * ((tl + tr + bl + br) - 4.0 * center);

    // Clamp values safely to preserve color validity
    finalColor = clamp(sharpened, 0.0, 1.0) * fragColor;
}