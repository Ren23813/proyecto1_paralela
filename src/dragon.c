
// Cabeza de dragon: una funcion por cada tipo de figura (circulo/ovalo,
// rectangulo redondeado, gota, estrella, linea ondulada, palito con rama)
// y drawDragonHeadArtColor()/drawDragonHeadArtFixed() que las combinan en
// dos capas (una tenible con el color del dragon, otra fija en negro/
// blanco/marron), EXACTAMENTE en las coordenadas originales del
// prototipo (nada de esto cambia con la animacion).
//
// Para poder mover/rotar/escalar/espejar la cabeza sin reescribir cada
// funcion de dibujo, se dibuja UNA sola vez por capa 

#include <math.h>
#include <stdlib.h>
#include "dragon.h"

// Los 4 colores del dragon (rojo, azul, amarillo, verde).
// Cada dragon nuevo toma el siguiente color de la lista (initDragon), y
// las 3 estrellas del estallido usan los OTROS 3 colores que le sobran
// a ese color de piel (initDragonHeadArt / renderDragon).
static const float DRAGON_PALETTE[4][3] = {
    { 214.0f,  32.0f,  38.0f }, // 0: rojo
    {  60.0f,  92.0f, 150.0f }, // 1: azul
    { 231.0f, 190.0f,  40.0f }, // 2: amarillo
    { 142.0f, 181.0f,  62.0f }, // 3: verde
};
#define DRAGON_PALETTE_SIZE 4

/* ===================================================================
 *  PRIMITIVAS GENERICAS (rellenar circulo, poligono, rectangulo, etc.)
 *  (initDragonHeadArt). No corren por frame, asi que no hace falta
 *  tocarlas.
 * =================================================================== */

/* Poligono relleno generico (scanline), usado por triangulo, estrella y gota */
static void fill_polygon(SDL_Renderer* ren, SDL_Point* pts, int n, SDL_Color col) {
    SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, col.a);
    int miny = pts[0].y, maxy = pts[0].y;
    for (int i = 1; i < n; i++) {
        if (pts[i].y < miny) miny = pts[i].y;
        if (pts[i].y > maxy) maxy = pts[i].y;
    }
    double* xs = malloc(sizeof(double) * n);
    for (int y = miny; y <= maxy; y++) {
        double yc = y + 0.5;
        int count = 0;
        for (int i = 0; i < n; i++) {
            SDL_Point a = pts[i], b = pts[(i + 1) % n];
            if ((a.y <= yc && b.y > yc) || (b.y <= yc && a.y > yc)) {
                double t = (yc - a.y) / (double)(b.y - a.y);
                xs[count++] = a.x + t * (b.x - a.x);
            }
        }
        for (int i = 1; i < count; i++) {
            double key = xs[i]; int j = i - 1;
            while (j >= 0 && xs[j] > key) { xs[j + 1] = xs[j]; j--; }
            xs[j + 1] = key;
        }
        for (int i = 0; i + 1 < count; i += 2) {
            int xa = (int)ceil(xs[i] - 0.5), xb = (int)floor(xs[i + 1] - 0.5);
            SDL_RenderDrawLine(ren, xa, y, xb, y);
        }
    }
    free(xs);
}

/* Óvalo relleno con ángulo de inclinación en grados */
static void fill_ellipse(SDL_Renderer* ren, int cx, int cy, int rx, int ry, double angle_deg, SDL_Color col) {
    double rad = angle_deg * M_PI / 180.0;
    double cos_a = cos(rad);
    double sin_a = sin(rad);

    const int N = 32; // suficiente para que no se note poligonal
    SDL_Point pts[32];
    for (int i = 0; i < N; i++) {
        double t = 2.0 * M_PI * i / N;
        double lx = rx * cos(t);
        double ly = ry * sin(t);
        pts[i].x = (int)round(cx + (lx * cos_a - ly * sin_a));
        pts[i].y = (int)round(cy + (lx * sin_a + ly * cos_a));
    }
    fill_polygon(ren, pts, N, col);
}

static void fill_circle(SDL_Renderer* ren, int cx, int cy, int r, SDL_Color col) {
    fill_ellipse(ren, cx, cy, r, r, 0.0, col);
}

/* Rectangulo con esquinas redondeadas = rect central + 4 circulos */
static void fill_rounded_rect(SDL_Renderer* ren, int x, int y, int w, int h, int radius, SDL_Color col) {
    SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, col.a);
    SDL_Rect vertical   = { x, y + radius, w, h - 2 * radius };
    SDL_Rect horizontal = { x + radius, y, w - 2 * radius, h };
    SDL_RenderFillRect(ren, &vertical);
    SDL_RenderFillRect(ren, &horizontal);
    fill_circle(ren, x + radius,         y + radius,         radius, col);
    fill_circle(ren, x + w - radius,     y + radius,         radius, col);
    fill_circle(ren, x + radius,         y + h - radius,     radius, col);
    fill_circle(ren, x + w - radius,     y + h - radius,     radius, col);
}

/* Estampa un circulo relleno a lo largo de un segmento (linea "gruesa") */
static void thick_line(SDL_Renderer* ren, double x0, double y0, double x1, double y1, int thickness, SDL_Color col) {
    double dist = hypot(x1 - x0, y1 - y0);
    int steps = (int)dist + 1;
    for (int s = 0; s <= steps; s++) {
        double t = (double)s / steps;
        fill_circle(ren, (int)(x0 + (x1 - x0) * t), (int)(y0 + (y1 - y0) * t), thickness / 2, col);
    }
}

/* ===================================================================
 *  FIGURAS DE LA CABEZA (una funcion por cada tipo de figura)
 * =================================================================== */

/* Estrella de N puntas (mostacilla amarilla, verde, estallido azul grande) */
static void draw_star(SDL_Renderer* ren, int cx, int cy, int outerR, int innerR,
                       int npoints, double rotationDeg, SDL_Color col) {
    int n = npoints * 2;
    SDL_Point* pts = malloc(sizeof(SDL_Point) * n);
    double rot = rotationDeg * M_PI / 180.0;
    for (int i = 0; i < n; i++) {
        double ang = rot + i * M_PI / npoints;
        int r = (i % 2 == 0) ? outerR : innerR;
        pts[i].x = cx + (int)(r * sin(ang));
        pts[i].y = cy - (int)(r * cos(ang));
    }
    fill_polygon(ren, pts, n, col);
    free(pts);
}

/* Curva Catmull-Rom, usada para suavizar la gota */
static SDL_Point catmull_rom(SDL_Point p0, SDL_Point p1, SDL_Point p2, SDL_Point p3, double t) {
    double t2 = t * t, t3 = t2 * t;
    SDL_Point r;
    r.x = (int)(0.5 * ((2 * p1.x) + (-p0.x + p2.x) * t + (2*p0.x - 5*p1.x + 4*p2.x - p3.x) * t2
                        + (-p0.x + 3*p1.x - 3*p2.x + p3.x) * t3));
    r.y = (int)(0.5 * ((2 * p1.y) + (-p0.y + p2.y) * t + (2*p0.y - 5*p1.y + 4*p2.y - p3.y) * t2
                        + (-p0.y + 3*p1.y - 3*p2.y + p3.y) * t3));
    return r;
}

/* Gota / lagrima: circulo por un lado que se afina en punta por el otro.
 * rotationDeg=0 -> la punta apunta hacia arriba. */
static void draw_teardrop(SDL_Renderer* ren, int cx, int cy, int size, double rotationDeg, SDL_Color col) {
    double keyLocal[8][2] = {
        {0.00,-1.20}, {0.55,-0.35}, {0.75, 0.35}, {0.40, 0.85},
        {0.00, 1.00}, {-0.40,0.85}, {-0.75, 0.35}, {-0.55,-0.35}
    };
    double rot = rotationDeg * M_PI / 180.0;
    SDL_Point key[8];
    for (int i = 0; i < 8; i++) {
        double lx = keyLocal[i][0] * size, ly = keyLocal[i][1] * size;
        key[i].x = cx + (int)(lx * cos(rot) - ly * sin(rot));
        key[i].y = cy + (int)(lx * sin(rot) + ly * cos(rot));
    }
    int stepsPerSeg = 12;
    SDL_Point out[8 * 12];
    int idx = 0;
    for (int i = 0; i < 8; i++) {
        SDL_Point p0 = key[(i - 1 + 8) % 8], p1 = key[i], p2 = key[(i + 1) % 8], p3 = key[(i + 2) % 8];
        for (int s = 0; s < stepsPerSeg; s++) {
            double t = (double)s / stepsPerSeg;
            out[idx++] = catmull_rom(p0, p1, p2, p3, t);
        }
    }
    fill_polygon(ren, out, idx, col);
}

/* Linea ondulada / zigzag, de (x0,y0) a (x1,y1), tipo diente de sierra */
static void draw_wavy_line(SDL_Renderer* ren, int x0, int y0, int x1, int y1,
                            double amplitude, double wavelength, int thickness, SDL_Color col) {
    double dist = hypot(x1 - x0, y1 - y0);
    double angle = atan2(y1 - y0, x1 - x0);
    int steps = (int)dist;
    double px = 0, py = 0;
    for (int s = 0; s <= steps; s++) {
        double t = s;
        double wave = amplitude * sin(2 * M_PI * t / wavelength);
        double bx = x0 + cos(angle) * t;
        double by = y0 + sin(angle) * t;
        double cx = bx - sin(angle) * wave;
        double cy = by + cos(angle) * wave;
        if (s > 0) thick_line(ren, px, py, cx, cy, thickness, col);
        px = cx; py = cy;
    }
}

/* Palito con una pequena rama (como una ramita/cuerno) */
static void draw_stick(SDL_Renderer* ren, int x0, int y0, double angleDeg, int length,
                        int thickness, SDL_Color col) {
    double a = angleDeg * M_PI / 180.0;
    double x1 = x0 + cos(a) * length;
    double y1 = y0 + sin(a) * length;
    thick_line(ren, x0, y0, x1, y1, thickness, col);
    double bx = x0 + (x1 - x0) * 0.55;
    double by = y0 + (y1 - y0) * 0.55;
    double a2 = a - 35 * M_PI / 180.0;
    double bx2 = bx + cos(a2) * (length * 0.4);
    double by2 = by + sin(a2) * (length * 0.4);
    thick_line(ren, bx, by, bx2, by2, thickness * 0.7, col);
}

/* ===================================================================
 *  CUERPO DEL DRAGON: rombo con degradado, dibujado DIRECTO sobre el
 *  FrameBuffer (no sobre SDL_Renderer). Recortado a [yStart, yEnd), asi
 *  que se puede llamar en paralelo por bandas de filas sin ningun lock.
 * =================================================================== */
void renderDragonBodySegment(FrameBuffer* fb, const Segment* seg,
                              double halfWidth, double halfHeight,
                              Uint8 r, Uint8 g, Uint8 b, int yStart, int yEnd) {
    // Mismo factor de oscurecimiento que tenian los colores fijos originales
    // (140/214 ~= 0.65), asi que el degradado se ve igual de "tallado".
    Uint8 lr = (Uint8)(r * 0.65f), lg = (Uint8)(g * 0.65f), lb = (Uint8)(b * 0.65f);
    fbFillDiamondGradient(fb, (int)seg->x, (int)seg->y,
                           halfWidth, halfHeight, seg->angle,
                           lr, lg, lb,   // borde izquierdo (oscuro)
                           r,  g,  b,    // borde derecho (color propio, claro)
                           yStart, yEnd);
}

/* 
 *  ENSAMBLAJE: la cabeza completa del dragon (coordenadas originales,
 *  SIN TOCAR -- esto es exactamente lo que ya te habia quedado bien)
 */

/* --- CAPA DE PIEL: hocico, cabeza, nariz y orejas (todo lo que antes era
 * "rojo"). Se dibuja en tonos de GRIS/BLANCO puro (no en rojo) para que,
 * al "teñirla" (multiplicar cada pixel por el color del dragon), de
 * verdad se pueda pintar de cualquiera de los 4 colores: multiplicar un
 * pixel blanco (255,255,255) por el tinte (r,g,b) da exactamente (r,g,b);
 * en cambio multiplicar un pixel ya rojo (214,32,38) por un tinte azul
 * solo puede oscurecer el rojo, nunca volverlo azul. */
static void drawDragonHeadArtSkin(SDL_Renderer* renderer) {
    SDL_Color full = { 255, 255, 255, 255 }; // se vuelve el color pleno del dragon
    SDL_Color dark = { 178, 178, 178, 255 }; // ~70% del color del dragon (interior oreja)

    /* --- Cuerpo principal: hocico + cabeza (rectangulos redondeados) --- */
    fill_rounded_rect(renderer, 220,  275, 400, 125, 30, full);  /* hocico alargado */
    fill_rounded_rect(renderer, 500, 200, 175, 200, 30, full);   /* bloque de la cabeza */

    /* nariz */
    fill_ellipse(renderer, 265, 275, 38, 50, 50, full);

    /* --- oreja --- */
    draw_teardrop(renderer, 680, 220, 55, 50, full);
    /* --- interior oreja (mas oscuro, como antes) --- */
    draw_teardrop(renderer, 690, 220, 30, 50, dark);
}

/* --- CAPA DE ESTRELLAS (la "melena"/estallido detras de la cabeza): NO se
 * tine en tiempo real. Como solo hay 4 colores posibles de piel, se
 * "hornean" 4 variantes distintas (una por color de piel) en
 * initDragonHeadArt(): cada variante usa los OTROS 3 colores de la
 * paleta que le sobran a ese color de piel, uno por anillo. Por eso esta
 * funcion recibe los 3 colores ya resueltos en vez de calcularlos. */
static void drawDragonHeadArtStars(SDL_Renderer* renderer,
                                    SDL_Color outer, SDL_Color mid, SDL_Color inner) {
    draw_star(renderer, 700, 300, 170, 140, 12, 0,  outer);
    draw_star(renderer, 700, 300, 110,  85,  8,  20, mid);
    draw_star(renderer, 700, 300, 85,  65,  8,  0, inner);
}

/* --- CAPA FIJA: ojo, dientes, fosa nasal, boca y cuerno. Estos SIEMPRE
 * se dibujan con su color real y nunca se tiñen (tinte 255,255,255 =
 * neutro), por eso el negro y el blanco no se alteran sin importar el
 * color que le toque al dragon. */
static void drawDragonHeadArtFixed(SDL_Renderer* renderer) {
    SDL_Color black    = { 20, 18, 16, 255 };
    SDL_Color white    = { 245, 245, 240, 255 };
    SDL_Color brown    = { 120, 72, 24, 255 };

    fill_rounded_rect(renderer, 220,  320, 280, 50, 20, white);  /* boca */

    /* --- Dientes en zigzag a lo largo de la boca --- */
    draw_wavy_line(renderer, 220, 350, 500, 350, 13, 50, 2, black);
    /* fosa nasal */
    fill_ellipse(renderer, 275, 285, 10, 16, 40, black);
    /* --- Ojo --- */
    fill_ellipse(renderer, 580, 270, 20, 35, 70, black);
    /* --- Cuernito tipo ramita en la parte de arriba de la cabeza --- */
    draw_stick(renderer, 640, 208, -110, 70, 8, brown);
}

/* ===================================================================
 *  ARTE DE LA CABEZA HORNEADO A RAM: se dibuja una sola vez (con SDL,
 *  reutilizando fill_polygon/fill_ellipse/etc como siempre) y se lee de
 *  vuelta a un arreglo de pixeles plano. De ahi en mas, cada frame, se
 *  dibuja rotada/escalada/teñida con fbBlitRotatedTinted, sin tocar SDL.
 * =================================================================== */

// Tamano del "lienzo" donde vive el dibujo de la cabeza (coincide con el
// canvas original del prototipo, para no cortar nada).
#define HEAD_TEX_W 900
#define HEAD_TEX_H 650

// Punto donde el cuello se une al cuerpo, EN COORDENADAS ORIGINALES del
// dibujo (el centro del estallido de estrellas). Este es el punto que se
// hace coincidir con (x, y) al llamar renderDragon(), y tambien el
// pivote de la rotacion.
#define NECK_ANCHOR_X 700.0f
#define NECK_ANCHOR_Y 300.0f

// Capa "de piel" (hocico, cabeza, nariz, orejas): se tine multiplicando
// por el color de cada dragon al dibujarla cada frame.
static Uint32* s_headSkinNormal = NULL;
static Uint32* s_headSkinMirror = NULL;
// Capa "de estrellas" (melena/estallido): 4 variantes horneadas, una por
// cada posible color de piel del dragon (indice 0..3 = DRAGON_PALETTE).
// Nunca se tine: cada variante ya trae los 3 colores reales que le
// sobran a ese color de piel.
static Uint32* s_headStarsNormal[DRAGON_PALETTE_SIZE] = { NULL };
static Uint32* s_headStarsMirror[DRAGON_PALETTE_SIZE] = { NULL };
// Capa "fija" (ojo, dientes, boca, fosa nasal, cuerno): jamas se tine.
static Uint32* s_headFixedNormal = NULL;
static Uint32* s_headFixedMirror = NULL;

// Dibuja drawFn en una textura SDL temporal y devuelve el resultado como
// un buffer de pixeles en RAM (RGBA8888, mismo empaquetado que
// FrameBuffer). Se llama UNA sola vez por capa, al arranque.
static Uint32* bakeHeadLayer(SDL_Renderer* renderer, void (*drawFn)(SDL_Renderer*)) {
    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET, HEAD_TEX_W, HEAD_TEX_H);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer, tex);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    drawFn(renderer);

    Uint32* pixels = malloc(sizeof(Uint32) * (size_t)HEAD_TEX_W * HEAD_TEX_H);
    SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_RGBA8888, pixels,
                          HEAD_TEX_W * (int)sizeof(Uint32));
    SDL_DestroyTexture(tex);
    return pixels;
}

// Igual que bakeHeadLayer, pero para la capa de estrellas, que necesita 3
// colores concretos (no una funcion sin argumentos).
static Uint32* bakeStarsLayer(SDL_Renderer* renderer, SDL_Color outer, SDL_Color mid, SDL_Color inner) {
    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET, HEAD_TEX_W, HEAD_TEX_H);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer, tex);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    drawDragonHeadArtStars(renderer, outer, mid, inner);

    Uint32* pixels = malloc(sizeof(Uint32) * (size_t)HEAD_TEX_W * HEAD_TEX_H);
    SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_RGBA8888, pixels,
                          HEAD_TEX_W * (int)sizeof(Uint32));
    SDL_DestroyTexture(tex);
    return pixels;
}

// Espejo horizontal de una capa ya horneada. Como ahora es solo un
// arreglo de pixeles, ni hace falta pasar por SDL_Renderer para esto.
static Uint32* mirrorHeadLayer(const Uint32* src) {
    Uint32* dst = malloc(sizeof(Uint32) * (size_t)HEAD_TEX_W * HEAD_TEX_H);
    for (int y = 0; y < HEAD_TEX_H; y++) {
        const Uint32* srow = src + (size_t)y * HEAD_TEX_W;
        Uint32* drow = dst + (size_t)y * HEAD_TEX_W;
        for (int x = 0; x < HEAD_TEX_W; x++) {
            drow[x] = srow[HEAD_TEX_W - 1 - x];
        }
    }
    return dst;
}

void initDragonHeadArt(SDL_Renderer* renderer) {
    if (s_headSkinNormal) return; // ya horneado

    SDL_Texture* prevTarget = SDL_GetRenderTarget(renderer);

    s_headSkinNormal  = bakeHeadLayer(renderer, drawDragonHeadArtSkin);
    s_headFixedNormal = bakeHeadLayer(renderer, drawDragonHeadArtFixed);
    s_headSkinMirror  = mirrorHeadLayer(s_headSkinNormal);
    s_headFixedMirror = mirrorHeadLayer(s_headFixedNormal);

    // Por cada posible color de piel, la melena usa los OTROS 3 colores
    // de la paleta (en el orden en que aparecen, saltandose el propio).
    for (int skinIdx = 0; skinIdx < DRAGON_PALETTE_SIZE; skinIdx++) {
        SDL_Color otherColors[DRAGON_PALETTE_SIZE - 1];
        int n = 0;
        for (int j = 0; j < DRAGON_PALETTE_SIZE; j++) {
            if (j == skinIdx) continue;
            otherColors[n].r = (Uint8)DRAGON_PALETTE[j][0];
            otherColors[n].g = (Uint8)DRAGON_PALETTE[j][1];
            otherColors[n].b = (Uint8)DRAGON_PALETTE[j][2];
            otherColors[n].a = 255;
            n++;
        }
        s_headStarsNormal[skinIdx] = bakeStarsLayer(renderer, otherColors[0], otherColors[1], otherColors[2]);
        s_headStarsMirror[skinIdx] = mirrorHeadLayer(s_headStarsNormal[skinIdx]);
    }

    SDL_SetRenderTarget(renderer, prevTarget);
}

// Dibuja la cabeza directo sobre el framebuffer usando las capas ya
// horneadas, con fbBlitRotatedTinted (rotacion/escala/tinte hechos a
// mano, sin SDL_Renderer). Recortado a [yStart, yEnd) -- se puede llamar
// en paralelo por bandas igual que el cuerpo.
static void renderDragonHeadToFB(FrameBuffer* fb, float x, float y, float scale, float angleDeg,
                                  Uint8 r, Uint8 g, Uint8 b, int colorIndex, int yStart, int yEnd) {
    // Normalizar el angulo a (-180, 180]
    float a = angleDeg;
    while (a > 180.0f)  a -= 360.0f;
    while (a <= -180.0f) a += 360.0f;

    // Convencion: 0 grados = mirando a la derecha, 180/-180 = izquierda
    // (igual que atan2(dy,dx) con Y hacia abajo). El dibujo original mira
    // a la izquierda, o sea "nace" en 180 grados.
    //
    // Para no rotar la cabeza 180 grados de golpe (lo que la dejaria de
    // cabeza), cuando el rumbo esta en la mitad derecha usamos la capa
    // ya espejada y le aplicamos solo la rotacion "sobrante" (que queda
    // siempre entre -90 y 90 grados, o sea nunca se voltea).
    int facingRight = (fabsf(a) < 90.0f);
    float artAngle;
    float anchorLocalX;

    if (facingRight) {
        artAngle = a;                              // ya cae en (-90, 90)
        anchorLocalX = HEAD_TEX_W - NECK_ANCHOR_X;   // el ancla tambien se espeja
    } else {
        artAngle = (a >= 0.0f) ? (a - 180.0f) : (a + 180.0f); // cae en (-90, 90]
        anchorLocalX = NECK_ANCHOR_X;
    }
    float anchorLocalY = NECK_ANCHOR_Y;

    const Uint32* stars = facingRight ? s_headStarsMirror[colorIndex] : s_headStarsNormal[colorIndex];
    const Uint32* skin  = facingRight ? s_headSkinMirror  : s_headSkinNormal;
    const Uint32* fixed = facingRight ? s_headFixedMirror : s_headFixedNormal;

    // Solo la piel recibe el tinte del dragon (hocico, cabeza, nariz,
    // orejas). Las estrellas ya vienen "horneadas" con sus 3 colores
    // reales y la capa fija (ojo, dientes, boca, cuerno) se pinta sin
    // tinte (255,255,255), para que negro y blanco no cambien.
    //
    // Orden: estrellas al fondo, piel encima (tapa el centro del
    // estallido), y la capa fija hasta arriba -- igual que antes.
    fbBlitRotatedTinted(fb, stars, HEAD_TEX_W, HEAD_TEX_H, anchorLocalX, anchorLocalY,
                         x, y, scale, artAngle, 255, 255, 255, yStart, yEnd);
    fbBlitRotatedTinted(fb, skin, HEAD_TEX_W, HEAD_TEX_H, anchorLocalX, anchorLocalY,
                         x, y, scale, artAngle, r, g, b, yStart, yEnd);
    fbBlitRotatedTinted(fb, fixed, HEAD_TEX_W, HEAD_TEX_H, anchorLocalX, anchorLocalY,
                         x, y, scale, artAngle, 255, 255, 255, yStart, yEnd);
}

/* ===================================================================
 *  DRAGON COMPLETO: movimiento tipo "vagabundeo" + cuerpo serpenteante
 * =================================================================== */

#define DRAGON_EDGE_MARGIN 90.0f       // si la cabeza entra aca, busca otro rumbo
#define DRAGON_TARGET_REACH_DIST 15.0f // que tan cerca del destino se considera "llegue"
#define DRAGON_WAVE_AMPLITUDE_DEG 22.0f   // que tan pronunciado es el zigzag
#define DRAGON_WAVE_WAVELENGTH    140.0f  // px que tarda en completarse un ciclo de onda

static float frand(float lo, float hi) {
    float r;
    #pragma omp critical(rng_lock)
    {
        r = lo + (hi - lo) * ((float)rand() / (float)RAND_MAX);
    }
    return r;
}

// Los 4 colores "oficiales" del dragon estan definidos al inicio del
// archivo (DRAGON_PALETTE), junto con DRAGON_PALETTE_SIZE.

static void pickNewTarget(Dragon* dragon, int windowW, int windowH) {
    float margin = DRAGON_EDGE_MARGIN + 20.0f;
    if (margin > windowW * 0.4f) margin = windowW * 0.4f;
    if (margin > windowH * 0.4f) margin = windowH * 0.4f;
    dragon->targetX = frand(margin, windowW - margin);
    dragon->targetY = frand(margin, windowH - margin);
}

void initDragon(Dragon* dragon, int numSegments, float segmentSpacing,
                 float startX, float startY, float speed,
                 int windowW, int windowH) {
    dragon->segments = malloc(sizeof(Segment) * numSegments);
    dragon->numSegments = numSegments;
    dragon->speed = speed;

    // Color del dragon: se alterna entre los 4 colores oficiales de la
    // paleta (rojo, azul, amarillo, verde) en vez de un tono aleatorio.
    static int s_paletteIndex = 0;
    int paletteIdx = s_paletteIndex % DRAGON_PALETTE_SIZE;
    dragon->r = DRAGON_PALETTE[paletteIdx][0];
    dragon->g = DRAGON_PALETTE[paletteIdx][1];
    dragon->b = DRAGON_PALETTE[paletteIdx][2];
    dragon->colorIndex = paletteIdx;
    s_paletteIndex++;

    dragon->segmentSpacing = segmentSpacing;
    dragon->chainDistAccum = 0.0f;
    dragon->waveDist = 0.0f;
    dragon->depth = frand(0.6f, 1.6f);

    float angle0 = frand(-180.0f, 180.0f);
    float rad0 = angle0 * (float)M_PI / 180.0f;

    // El cuerpo arranca estirado hacia atras de la cabeza, en linea recta,
    // para que la primera "herencia" de posiciones no de un salto raro.
    for (int i = 0; i < numSegments; i++) {
        dragon->segments[i].x = startX - cosf(rad0) * segmentSpacing * i;
        dragon->segments[i].y = startY - sinf(rad0) * segmentSpacing * i;
        dragon->segments[i].angle = angle0;
    }

    pickNewTarget(dragon, windowW, windowH);
}

void freeDragon(Dragon* dragon) {
    free(dragon->segments);
    dragon->segments = NULL;
    dragon->numSegments = 0;
}

void updateDragon(Dragon* dragon, float dt, int windowW, int windowH) {
    Segment* head = &dragon->segments[0];

    // 1) Decidir si hace falta un nuevo destino: o llegamos, o nos
    //    acercamos demasiado al borde de la ventana.
    float dx = dragon->targetX - head->x;
    float dy = dragon->targetY - head->y;
    float distToTarget = sqrtf(dx * dx + dy * dy);

    int nearEdge = (head->x < DRAGON_EDGE_MARGIN || head->x > windowW - DRAGON_EDGE_MARGIN ||
                     head->y < DRAGON_EDGE_MARGIN || head->y > windowH - DRAGON_EDGE_MARGIN);

    if (distToTarget < DRAGON_TARGET_REACH_DIST || nearEdge) {
        pickNewTarget(dragon, windowW, windowH);
        dx = dragon->targetX - head->x;
        dy = dragon->targetY - head->y;
        distToTarget = sqrtf(dx * dx + dy * dy);
    }

    // 2) Guardamos el estado de la cabeza ANTES de moverla: es lo que va
    //    a heredar el primer segmento del cuerpo cuando corresponda.
    Segment prevHead = *head;

    float headAngleRad = atan2f(dy, dx);
    float step = dragon->speed * dt;
    if (step > distToTarget) step = distToTarget;

    // Le sumamos un angulo extra que oscila segun la distancia recorrida,
    // para que la cabeza (y el cuerpo, que la sigue) serpentee.
    float wobble = (DRAGON_WAVE_AMPLITUDE_DEG * (float)M_PI / 180.0f) *
                sinf(2.0f * (float)M_PI * dragon->waveDist / DRAGON_WAVE_WAVELENGTH);
    float moveAngleRad = headAngleRad + wobble;

    head->x += cosf(moveAngleRad) * step;
    head->y += sinf(moveAngleRad) * step;
    head->angle = moveAngleRad * 180.0f / (float)M_PI;
    dragon->waveDist += step;
    dragon->chainDistAccum += step;


    while (dragon->chainDistAccum >= dragon->segmentSpacing && dragon->numSegments > 1) {
        for (int i = dragon->numSegments - 1; i >= 1; i--) {
            dragon->segments[i] = (i == 1) ? prevHead : dragon->segments[i - 1];
        }
        dragon->chainDistAccum -= dragon->segmentSpacing;
    }
}

// Dibuja el dragon completo (cuerpo + cabeza) directo al framebuffer,
// recortado a la banda [yStart, yEnd). No toca SDL_Renderer para nada --
// se puede llamar en paralelo, una banda por hilo, y se puede intercalar
// libremente con fuegos artificiales y faroles respetando la profundidad.
void renderDragon(FrameBuffer* fb, const Dragon* dragon,
                   double headScale, double bodyHalfWidth, double bodyHalfHeight,
                   int yStart, int yEnd) {
    Uint8 r = (Uint8)dragon->r, g = (Uint8)dragon->g, b = (Uint8)dragon->b;
    float depthScale = 1.0f / dragon->depth;

    for (int i = dragon->numSegments - 1; i >= 1; i--) {
        renderDragonBodySegment(fb, &dragon->segments[i],
                                 bodyHalfWidth * depthScale, bodyHalfHeight * depthScale,
                                 r, g, b, yStart, yEnd);
    }

    const Segment* head = &dragon->segments[0];
    renderDragonHeadToFB(fb, head->x, head->y, (float)(headScale * depthScale), head->angle,
                          r, g, b, dragon->colorIndex, yStart, yEnd);
}
