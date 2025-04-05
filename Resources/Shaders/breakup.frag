#version 120
uniform float tilesX;           // numero di tiles orizzontali
uniform float tilesY;           // numero di tiles verticali
uniform int breakupCellforms;   // numero di cellforms (usato per calcolare l'indice)
uniform sampler2D tex;          // texture di base
uniform sampler2D tex1;         // texture dei cellforms
uniform sampler2D tex2;         // texture griglia (valori di raggio)

varying /* PRAGMA: ONS_RU highprecision */ vec2 texCoord;

void main(void) {
    // Calcola a quale tile appartiene il frammento
    float belongsToTileX = floor(texCoord.s * tilesX) / tilesX;
    float belongsToTileY = floor(texCoord.t * tilesY) / tilesY;
    float gridReportedRadius = texture2D(tex2, vec2(belongsToTileX, belongsToTileY)).r;
    
    if (gridReportedRadius >= 1.0) {
        // Se il raggio segnalato è massimo, usa il vecchio output senza modifiche
        gl_FragColor = texture2D(tex, texCoord.st);
    } else {
        // Calcola il risultato classico (breakup)
        int thisRadius = int(floor(gridReportedRadius * float(breakupCellforms)));
        float xPercentageThroughTile = mod(texCoord.s, 1.0/tilesX) * tilesX;
        float yPercentageThroughTile = mod(texCoord.t, 1.0/tilesY) * tilesY;
        float breakupCellformsInterval = 1.0/float(breakupCellforms);
        float breakupCellformsStartX = float(thisRadius-1) * breakupCellformsInterval;
        float x = breakupCellformsStartX + (breakupCellformsInterval * xPercentageThroughTile);
        float y = yPercentageThroughTile;
        vec4 classicBreakup = texture2D(tex1, vec2(x,y)).r * texture2D(tex, texCoord.st);
        
        // Calcola le coordinate relative della cella (da 0 a 1)
        vec2 cellCoord = vec2(xPercentageThroughTile, yPercentageThroughTile);
        vec2 center = vec2(0.5, 0.5);
        float d = distance(cellCoord, center);
        
        // Parametri per il cerchio dorato e il glow
        float innerRadius = 0.3;     // raggio interno pieno
        float outerRadius = 0.35;    // inizio dissolvenza del bordo
        float glowOuterRadius = 0.4; // raggio massimo del glow
        
        // Calcola l'opacità del cerchio e del glow
        float circleAlpha = 1.0 - smoothstep(innerRadius, outerRadius, d);
        float glowAlpha = 1.0 - smoothstep(outerRadius, glowOuterRadius, d);
        float goldenAlpha = max(circleAlpha, glowAlpha * 0.5);
        
        // Colore dorato
        vec3 golden = vec3(1.0, 0.84, 0.0);
        vec4 goldenOverlay = vec4(golden, goldenAlpha);
        
        // Miscelazione: il risultato classico viene "mascherato" dall'overlay dorato
        gl_FragColor = mix(classicBreakup, goldenOverlay, goldenAlpha);
    }
}
