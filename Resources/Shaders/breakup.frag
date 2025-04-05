#version 120
uniform float tilesX; // numero di tiles orizzontali
uniform float tilesY; // numero di tiles verticali
uniform int breakupCellforms; // non più usato in questo ramo, ma lasciato per compatibilità
uniform sampler2D tex;  // texture di sfondo
uniform sampler2D tex1; // texture originale (non utilizzata ora)
uniform sampler2D tex2; // texture griglia contenente il raggio per ogni tile

varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;

void main(void) {
    float belongsToTileX = floor(texCoord.s * tilesX) / tilesX;
    float belongsToTileY = floor(texCoord.t * tilesY) / tilesY;
    float gridReportedRadius = texture2D(tex2, vec2(belongsToTileX, belongsToTileY)).r;
    
    if (gridReportedRadius >= 1.0) {
        gl_FragColor = texture2D(tex, texCoord.st);
    } else {
        // Calcola coordinate all'interno della cella (da 0 a 1)
        float xPercentageThroughTile = mod(texCoord.s, 1.0/tilesX) * tilesX;
        float yPercentageThroughTile = mod(texCoord.t, 1.0/tilesY) * tilesY;
        vec2 cellCoord = vec2(xPercentageThroughTile, yPercentageThroughTile);
        
        // Centro della cella
        vec2 center = vec2(0.5, 0.5);
        float d = distance(cellCoord, center);
        
        // Parametri per cerchio e glow
        float innerRadius = 0.3;     // raggio interno: area piena
        float outerRadius = 0.35;    // inizio dissolvenza del bordo
        float glowOuterRadius = 0.4; // raggio massimo del glow
        
        // Calcola l'opacità per il cerchio interno
        float circle = 1.0 - smoothstep(innerRadius, outerRadius, d);
        // Calcola il glow, meno intenso
        float glow = 1.0 - smoothstep(outerRadius, glowOuterRadius, d);
        
        // Combina il cerchio con il glow (questo valore controlla la miscelazione)
        float finalAlpha = max(circle, glow * 0.5);
        
        // Colore dorato
        vec3 golden = vec3(1.0, 0.84, 0.0);
        
        // Sfondo di base
        vec4 bg = texture2D(tex, texCoord.st);
        // Miscelazione: nei pixel dove il glow è attivo il colore dorato si applica in modo trasparente
        gl_FragColor = mix(bg, vec4(golden, 1.0), finalAlpha);
    }
}
