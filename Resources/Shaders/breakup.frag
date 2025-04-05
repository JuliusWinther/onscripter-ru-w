#version 120
uniform float tilesX; // numero di tiles che si adattano orizzontalmente
uniform float tilesY; // numero di tiles che si adattano verticalmente
uniform int breakupCellforms; // numero di cellforms in tex1
uniform sampler2D tex;  // texture contenente la superficie originale
uniform sampler2D tex1; // texture contenente una riga orizzontale di cellforms crescenti
uniform sampler2D tex2; // texture griglia (tilesX * tilesY) con il valore del raggio (0 = nessun raggio, 1.0 = raggio pieno)

varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;

void main(void) {
    float belongsToTileX = floor(texCoord.s * tilesX) / tilesX;
    float belongsToTileY = floor(texCoord.t * tilesY) / tilesY;
    float gridReportedRadius = texture2D(tex2, vec2(belongsToTileX, belongsToTileY)).r;
    
    if (gridReportedRadius >= 1.0) {
        gl_FragColor = texture2D(tex, texCoord);
    } else {
        // Calcola la dimensione e il centro della cella corrente
        vec2 tileSize = vec2(1.0 / tilesX, 1.0 / tilesY);
        vec2 tileCenter = vec2(belongsToTileX + tileSize.x * 0.5, belongsToTileY + tileSize.y * 0.5);
        
        // Calcola la distanza dal centro della cella
        float dist = distance(texCoord, tileCenter);
        
        // Parametri per il cerchio e l'effetto glow
        float circleRadius = tileSize.x * 0.4;       // raggio del cerchio (puoi modificarlo)
        float glowThickness = circleRadius * 0.3;      // spessore dell'alone (glow)
        
        // Crea la maschera del cerchio con bordo netto
        float circleMask = smoothstep(circleRadius, circleRadius - 0.01, dist);
        // Calcola il glow come una sfumatura attorno al bordo del cerchio
        float glow = smoothstep(circleRadius + glowThickness, circleRadius, dist) - circleMask;
        
        // Colore dorato
        vec3 golden = vec3(1.0, 0.84, 0.0);
        // Campiona il colore originale per un possibile mix (oppure, se preferisci, puoi usare solo il dorato)
        vec4 baseColor = texture2D(tex, texCoord);
        // Mescola il colore originale con il dorato all'interno del cerchio e aggiungi il glow
        vec3 color = mix(baseColor.rgb, golden, circleMask) + golden * glow;
        
        gl_FragColor = vec4(color, baseColor.a);
    }
}
