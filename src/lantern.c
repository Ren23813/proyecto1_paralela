#include <math.h>
#include "lantern.h"

// CONSTANTES Y CONFIGURACIÓN GEOMÉTRICA DE LA PLANTILLA
#define LANTERN_SCALE 0.12f  // Escala global de reducción para la plantilla base
#define REF_X 250.0f        // Centro de referencia X del dibujo vectorial original
#define REF_Y 340.0f        // Centro de referencia Y del dibujo vectorial original

// Puntos clave e interpolación para la silueta del cuerpo
#define BODY_KEY_COUNT 10
#define STEPS_PER_SEG  4
#define BODY_POINT_COUNT (BODY_KEY_COUNT * STEPS_PER_SEG)

// Varillas internas
#define RIB_COUNT 5
#define RIB_STEPS 12

//flecos inferiores
#define TASSEL_COUNT 5
#define TASSEL_STEPS 10

// Estructura vectorial bidimensional auxiliar
typedef struct { float x, y; } Vec2;

// Plantilla global: misma forma para todos los faroles
//(Precalculadas una sola vez)
static Vec2 bodyOutline[BODY_POINT_COUNT];
static Vec2 ribPoints[RIB_COUNT][RIB_STEPS + 1];
static Vec2 tasselPoints[TASSEL_COUNT][TASSEL_STEPS + 1];
static Vec2 topCap[4], botCap[4];
static Vec2 ringCenter, knotCenter, stringTop, stringBottom;
static float ringRadius, knotRadius;


// MATEMÁTICAS VECTORIALES Y CURVAS DE INTERPOLACIÓN
// Normaliza y escala un punto desde el espacio de referencia original
// al espacio local centrado de la plantilla del farol.
static Vec2 localP(float x, float y) {
    Vec2 v;
    v.x = (x - REF_X) * LANTERN_SCALE;
    v.y = (y - REF_Y) * LANTERN_SCALE;
    return v;
}

//Produce curvas suaves que pasan exactamente por los puntos de control intermedios (p1 y p2).
static Vec2 catmullRom(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float t) {
    float t2 = t*t, t3 = t2*t;
    Vec2 r;
    r.x = 0.5f*((2*p1.x) + (-p0.x+p2.x)*t + (2*p0.x-5*p1.x+4*p2.x-p3.x)*t2 + (-p0.x+3*p1.x-3*p2.x+p3.x)*t3);
    r.y = 0.5f*((2*p1.y) + (-p0.y+p2.y)*t + (2*p0.y-5*p1.y+4*p2.y-p3.y)*t2 + (-p0.y+3*p1.y-3*p2.y+p3.y)*t3);
    return r;
}

//Utilizada para las varillas y borlas.
static Vec2 bezierCubic(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float t) {
    float u = 1-t, b0=u*u*u, b1=3*u*u*t, b2=3*u*t*t, b3=t*t*t;
    Vec2 r;
    r.x = b0*p0.x + b1*p1.x + b2*p2.x + b3*p3.x;
    r.y = b0*p0.y + b1*p1.y + b2*p2.y + b3*p3.y;
    return r;
}

// CONSTRUCCIÓN DE LA PLANTILLA


//Precalcula la geometría completa del farol en espacio local.
//Se ejecuta una sola vez al inicio para evitar reevaluar curvas complejas en cada frame.
void buildLanternTemplate(void) {

    // Silueta: Reducida en altura (Y: 200 a 480) y ensanchada a los lados (X: 80 a 420)
    Vec2 bodyKey[BODY_KEY_COUNT] = {
        localP(180, 200), localP(110, 250), localP(80,  340), localP(110, 430),
        localP(180, 480), localP(320, 480), localP(390, 430),
        localP(420, 340), localP(390, 250), localP(320, 200)
    };
    int idx = 0;
    for (int i = 0; i < BODY_KEY_COUNT; i++) {
        Vec2 p0 = bodyKey[(i-1+BODY_KEY_COUNT) % BODY_KEY_COUNT];
        Vec2 p1 = bodyKey[i];
        Vec2 p2 = bodyKey[(i+1) % BODY_KEY_COUNT];
        Vec2 p3 = bodyKey[(i+2) % BODY_KEY_COUNT];
        for (int s = 0; s < STEPS_PER_SEG; s++)
            bodyOutline[idx++] = catmullRom(p0, p1, p2, p3, (float)s / STEPS_PER_SEG);
    }

    // Tapas ajustadas al nuevo ancho y posición vertical (Y=200 y Y=480)
    Vec2 tc[4] = { localP(200, 160), localP(300, 160), localP(320, 200), localP(180, 200) };
    Vec2 bc[4] = { localP(180, 480), localP(320, 480), localP(300, 520), localP(200, 520) };
    for (int i = 0; i < 4; i++) { topCap[i] = tc[i]; botCap[i] = bc[i]; }

    // Varillas o costillas de madera (Curvas Bézier Cúbicas)
    float ribTopX[] = {195, 222, 250, 278, 305};
    float ribMidX[] = {110, 180, 250, 320, 390}; 
    float ribBotX[] = {195, 222, 250, 278, 305};
    for (int i = 0; i < RIB_COUNT; i++) {
        Vec2 p0 = localP(ribTopX[i], 205), p1 = localP(ribMidX[i], 260);
        Vec2 p2 = localP(ribMidX[i], 420), p3 = localP(ribBotX[i], 475);
        for (int s = 0; s <= RIB_STEPS; s++)
            ribPoints[i][s] = bezierCubic(p0, p1, p2, p3, (float)s / RIB_STEPS);
    }

    // ubicación del aro, hilo y nudo según la nueva altura reducida
    ringCenter = localP(250, 125);   ringRadius = 14 * LANTERN_SCALE;

    // 5. Borlas colgantes inferiores (Curvas Bézier Cúbicas)
    float tasselX[] = {215, 233, 250, 267, 285};
    for (int i = 0; i < TASSEL_COUNT; i++) {
        float dx = tasselX[i] - 250;
        Vec2 p0 = localP(250, 540), p1 = localP(250 + dx*0.6f, 575);
        Vec2 p2 = localP(tasselX[i], 605), p3 = localP(tasselX[i], 640);
        for (int s = 0; s <= TASSEL_STEPS; s++)
            tasselPoints[i][s] = bezierCubic(p0, p1, p2, p3, (float)s / TASSEL_STEPS);
    }
}

// Actualiza la posición en pantalla del farol aplicando un movimiento
// oscilatorio armónico (curvas Lissajous con senos/cosenos) y deriva vertical opcional.
void updateLantern(Lantern* lantern, float elapsedTime) {
    lantern->x = lantern->baseX + lantern->amplitudeX * sinf(lantern->frequency * elapsedTime + lantern->phase);
    lantern->y = lantern->baseY + lantern->amplitudeY * cosf(lantern->frequency * elapsedTime + lantern->phase)
                 - lantern->driftY * elapsedTime;
}

// RENDERIZADO POR BANDAS (FRAMEBUFFER)

// Transforma y proyecta la plantilla del farol a la posición del mundo (cx, cy).
// Aplica escalado de profundidad (1 / depth) y renderiza cada componente en el
// framebuffer dentro de la banda vertical [yStart, yEnd) habilitada para multihilo.
void renderLantern(FrameBuffer* fb, const Lantern* lantern, int yStart, int yEnd) {
    float cx = lantern->x, cy = lantern->y;
    float depthScale = 1.0f / lantern->depth; // Factor de perspectiva

    //Renderizado del cuerpo principal (Polígono relleno rojo)
    SDL_Point body[BODY_POINT_COUNT];
    for (int i = 0; i < BODY_POINT_COUNT; i++) {
        body[i].x = (int)(cx + bodyOutline[i].x * depthScale);
        body[i].y = (int)(cy + bodyOutline[i].y * depthScale);
    }
    fbFillPolygon(fb, body, BODY_POINT_COUNT, 199, 32, 38, 255, yStart, yEnd);

    //Renderizado de las tapas (Polígonos dorados)
    SDL_Point tcap[4], bcap[4];
    for (int i = 0; i < 4; i++) {
        tcap[i].x = (int)(cx + topCap[i].x * depthScale); tcap[i].y = (int)(cy + topCap[i].y * depthScale);
        bcap[i].x = (int)(cx + botCap[i].x * depthScale); bcap[i].y = (int)(cy + botCap[i].y * depthScale);
    }
    fbFillPolygon(fb, tcap, 4, 226, 171, 60, 255, yStart, yEnd);
    fbFillPolygon(fb, bcap, 4, 226, 171, 60, 255, yStart, yEnd);

    //Renderizado de las varillas (Líneas oscuras)
    for (int r = 0; r < RIB_COUNT; r++)
        for (int s = 0; s < RIB_STEPS; s++)
            fbThickLine(fb,
                cx + ribPoints[r][s].x   * depthScale, cy + ribPoints[r][s].y   * depthScale,
                cx + ribPoints[r][s+1].x * depthScale, cy + ribPoints[r][s+1].y * depthScale,
                2, 150, 18, 20, 255, yStart, yEnd);

    //Renderizado del hilo y aro de sujeción
    fbThickLine(fb, cx + stringTop.x * depthScale, cy + stringTop.y * depthScale,
                    cx + stringBottom.x * depthScale, cy + stringBottom.y * depthScale,
                    2, 176, 120, 28, 255, yStart, yEnd);

    fbCircleOutline(fb, (int)(cx + ringCenter.x * depthScale), (int)(cy + ringCenter.y * depthScale),
                     (int)(ringRadius * depthScale), 176, 120, 28, 255, yStart, yEnd);

    fbFillCircle(fb, (int)(cx + knotCenter.x * depthScale), (int)(cy + knotCenter.y * depthScale),
                 (int)(knotRadius * depthScale), 176, 120, 28, 255, yStart, yEnd);

    //Renderizado de las borlas (Líneas doradas)
    for (int t = 0; t < TASSEL_COUNT; t++)
        for (int s = 0; s < TASSEL_STEPS; s++)
            fbThickLine(fb,
                cx + tasselPoints[t][s].x   * depthScale, cy + tasselPoints[t][s].y   * depthScale,
                cx + tasselPoints[t][s+1].x * depthScale, cy + tasselPoints[t][s+1].y * depthScale,
                1, 226, 171, 60, 255, yStart, yEnd);
}