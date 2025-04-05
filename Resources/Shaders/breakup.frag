#version 120
uniform float tilesX;            // Numero di tiles lungo l'asse orizzontale
uniform float tilesY;            // Numero di tiles lungo l'asse verticale
uniform int breakupCellforms;    // (Non usato in questo ramo, ma lasciato per retrocompatibilità)
uniform sampler2D tex;           // Texture di base
uniform sampler2D tex1;          // (Non usato in questo esempio)
uniform sampler2D tex2;          // Texture griglia contenente valori per il raggio (0 = effetto attivo, 1 = effetto disattivato)

varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;

void main(void) {
    // Calcola a quale cella appartiene il frammento
    float belongsToTileX = floor(texCoord.s * tilesX) / tilesX;
    float belongsToTileY = floor(texCoord.t * tilesY) / tilesY;
    float gridReportedRadius = texture2D(tex2, vec2(belongsToTileX, belongsToTileY)).r;
    
    // Se il valore è 1.0 o maggiore, si usa la texture originale
    if (gridReportedRadius >= 1.0) {
        gl_FragColor = texture2D(tex, texCoord);
    } else {
        // Calcola la dimensione della cella e il suo centro
        vec2 tileSize = vec2(1.0 / tilesX, 1.0 / tilesY);
        vec2 tileCenter = vec2(belongsToTileX + tileSize.x * 0.5, belongsToTileY + tileSize.y * 0.5);
        // Distanza dal centro della cella
        float dist = distance(texCoord, tileCenter);
        
        // Parametri per rendere l'effetto molto evidente:
        float circleRadius = tileSize.x * 0.45;   // Raggio del cerchio, quasi metà della larghezza della cella
        float glowThickness = circleRadius * 0.5;   // Uno spessore maggiore per l'alone
        
        // Maschera del cerchio con bordo netto (smoothstep per transizione)
        float circleMask = smoothstep(circleRadius, circleRadius - 0.05, dist);
        // Maschera per l'alone: transizione dall'esterno del cerchio fino al bordo esterno del glow
        float glowMask = smoothstep(circleRadius + glowThickness, circleRadius, dist) - circleMask;
        
        // Aumenta l'intensità del glow
        float glowIntensity = 3.0;
        // Colore dorato marcato
        vec3 golden = vec3(1.0, 0.84, 0.0);
        // Campiona il colore di base dalla texture
        vec4 baseColor = texture2D(tex, texCoord);
        
        // Combina il colore di base con il dorato, applicando sia il cerchio che il glow
        vec3 color = mix(baseColor.rgb, golden, circleMask) + golden * glowMask * glowIntensity;
        
        gl_FragColor = vec4(color, baseColor.a);
    }
}
