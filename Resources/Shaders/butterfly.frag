#version 120

// This shader is used by the butterfly breakup effect to tint the drawn butterflies
// with a brilliant, golden hue and to add a soft radial glow.
// The uniform "butterflyColor" should be set from the C++ code (e.g. to vec4(0.83, 0.69, 0.22, 1.0)).

uniform vec4 butterflyColor;
varying vec2 texCoord;

void main(void) {
    // Use the center of the butterfly piece as (0.5, 0.5)
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(texCoord, center);
    
    // Compute a smooth gradient (shine): bright in the center, fading outwards.
    float shine = smoothstep(0.5, 0.0, dist);
    
    // Base golden color modulated by the shine (0.7 base + 0.3 * shine)
    vec3 baseColor = butterflyColor.rgb * (0.7 + 0.3 * shine);
    
    // Add a specular highlight for extra glimmer.
    float specular = pow(shine, 10.0);
    vec3 specColor = vec3(0.2, 0.2, 0.1) * specular;
    
    vec3 finalColor = baseColor + specColor;
    
    gl_FragColor = vec4(finalColor, butterflyColor.a);
}
