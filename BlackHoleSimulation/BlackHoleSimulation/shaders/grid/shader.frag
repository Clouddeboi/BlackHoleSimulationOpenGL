#version 430 core

in vec3 vPos;
out vec4 FragColor;

uniform vec3 uGridColor = vec3(0.5, 0.5, 0.5);
uniform vec3 uBgColor   = vec3(0.0, 0.0, 0.0);
uniform float uScale    = 1.0;   // grid cell size
uniform float uThickness = 0.02; // line thickness
uniform float uFadeStart = 20.0;
uniform float uFadeEnd   = 100.0;

void main()
{
    // Project world position onto XZ plane
    vec2 gridCoord = vPos.xz / uScale;

    // Find distance to nearest grid line
    vec2 gridFrac = abs(fract(gridCoord - 0.5) - 0.5);
    float lineDist = min(gridFrac.x, gridFrac.y);

    // Base grid intensity
    float line = smoothstep(uThickness, 0.0, lineDist);

    // Fade with distance from camera
    float dist = length(vPos);
    float fade = 1.0 - smoothstep(uFadeStart, uFadeEnd, dist);

    // Mix colors
    vec3 color = mix(uBgColor, uGridColor, line * fade);
    FragColor = vec4(color, 1.0);
}
