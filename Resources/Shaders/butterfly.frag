#version 120

// This shader is used by the butterfly breakup effect to render each butterfly piece
// with a rich, golden, shining appearance.
// The golden color is provided by the uniform butterflyColor (e.g. vec4(0.83, 0.69, 0.22, 1.0)),
// which you should set from the effect code.
// The shader uses the interpolated texture coordinate (texCoord) to compute a radial gradient,
// giving the wings and body a soft center glow and a specular highlight.

uniform vec4 butterflyColor;
varying vec2 texCoord;

void main(void) {
    // Use (0.5,0.5) as the center of the butterfly piece.
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(texCoord, center);
    
    // Compute a smooth shine factor: bright in the center, fading outwards.
    float shine = smoothstep(0.5, 0.0, dist);
    
    // Base color is the golden butterflyColor, darkened slightly at the edges.
    vec3 baseColor = butterflyColor.rgb * (0.7 + 0.3 * shine);
    
    // Add a specular (gloss) component for extra shine.
    float specular = pow(shine, 10.0);
    vec3 specColor = vec3(0.2, 0.2, 0.1) * specular;
    
    vec3 finalColor = baseColor + specColor;
    
    gl_FragColor = vec4(finalColor, butterflyColor.a);
}
