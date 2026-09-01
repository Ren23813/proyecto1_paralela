// src/dragon.c
//
// Cabeza de dragon: una funcion por cada tipo de figura (circulo/ovalo,
// rectangulo redondeado, gota, estrella, linea ondulada, palito con rama)
// y drawDragonHeadArtColor()/drawDragonHeadArtFixed() que las combinan en
// dos capas (una tenible con el color del dragon, otra fija en negro/
// blanco/marron), EXACTAMENTE en las coordenadas originales del
// prototipo (nada de esto cambia con la animacion).
//
// Para poder mover/rotar/escalar/espejar la cabeza sin reescribir cada
// funcion de dibujo (que no soportan rotacion arbitraria), la dibujamos
// UNA sola vez a una textura, y despues usamos SDL_RenderCopyEx para
// posicionarla/rotarla/escalarla en cada frame. Ver ensureHeadTextures()
// y renderDragonHead() mas abajo.

#include <math.h>
#include <stdlib.h>
#include "dragon.h"

// Los 4 colores "oficiales" del dragon (rojo, azul, amarillo, verde).
// Cada dragon nuevo toma el siguiente color de la lista (initDragon), y
// las 3 estrellas del estallido usan los OTROS 3 colores que le sobran
// a ese color de piel (ensureHeadTextures / renderDragonHead).
static const float DRAGON_PALETTE[4][3] = {
    { 214.0f,  32.0f,  38.0f }, // 0: rojo
    {  60.0f,  92.0f, 150.0f }, // 1: azul
    { 231.0f, 190.0f,  40.0f }, // 2: amarillo
    { 142.0f, 181.0f,  62.0f }, // 3: verde
};
#define DRAGON_PALETTE_SIZE 4

/* ===================================================================
 *  PRIMITIVAS GENERICAS (rellenar circulo, poligono, rectangulo, etc.)
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

/* Rombo (diamante) relleno con degradado horizontal en espacio LOCAL:
 * de colorLeft (borde izquierdo, x local = -halfWidth) a colorRight
 * (borde derecho, x local = +halfWidth), antes de rotar. cx,cy es el
 * centro del rombo; halfWidth/halfHeight son las semi-diagonales;
 * angle_deg lo orienta (pensado para el campo `angle` de un Segment).
 *
 * Independiente de la cabeza: no se llama desde drawDragonHeadArtColor/Fixed. */
static void fill_diamond_gradient(SDL_Renderer* ren, int cx, int cy,
                                   double halfWidth, double halfHeight,
                                   double angle_deg,
                                   SDL_Color colorLeft, SDL_Color colorRight) {
    double rad = angle_deg * M_PI / 180.0;
    double cos_a = cos(rad),  sin_a = sin(rad);
    double cos_ai = cos(-rad), sin_ai = sin(-rad); // rotacion inversa (para el degradado)

    double localX[4] = { 0.0,  halfWidth, 0.0, -halfWidth };
    double localY[4] = { -halfHeight, 0.0, halfHeight, 0.0 };

    SDL_Point pts[4];
    int miny = INT32_MAX, maxy = INT32_MIN;
    for (int i = 0; i < 4; i++) {
        double wx = localX[i] * cos_a - localY[i] * sin_a;
        double wy = localX[i] * sin_a + localY[i] * cos_a;
        pts[i].x = (int)round(cx + wx);
        pts[i].y = (int)round(cy + wy);
        if (pts[i].y < miny) miny = pts[i].y;
        if (pts[i].y > maxy) maxy = pts[i].y;
    }

    for (int y = miny; y <= maxy; y++) {
        double yc = y + 0.5;
        double xs[2];
        int count = 0;
        for (int i = 0; i < 4 && count < 2; i++) {
            SDL_Point a = pts[i], b = pts[(i + 1) % 4];
            if ((a.y <= yc && b.y > yc) || (b.y <= yc && a.y > yc)) {
                double t = (yc - a.y) / (double)(b.y - a.y);
                xs[count++] = a.x + t * (b.x - a.x);
            }
        }
        if (count < 2) continue;
        if (xs[0] > xs[1]) { double tmp = xs[0]; xs[0] = xs[1]; xs[1] = tmp; }

        int xa = (int)ceil(xs[0] - 0.5), xb = (int)floor(xs[1] - 0.5);
        for (int x = xa; x <= xb; x++) {
            double dx = x - cx, dy = y - cy;
            double lx = dx * cos_ai - dy * sin_ai;
            double f = (lx + halfWidth) / (2.0 * halfWidth);
            if (f < 0) f = 0; else if (f > 1) f = 1;

            Uint8 r = (Uint8)(colorLeft.r + (colorRight.r - colorLeft.r) * f);
            Uint8 g = (Uint8)(colorLeft.g + (colorRight.g - colorLeft.g) * f);
            Uint8 b = (Uint8)(colorLeft.b + (colorRight.b - colorLeft.b) * f);
            Uint8 a = (Uint8)(colorLeft.a + (colorRight.a - colorLeft.a) * f);

            SDL_SetRenderDrawColor(ren, r, g, b, a);
            SDL_RenderDrawPoint(ren, x, y);
        }
    }
}

void renderDragonBodySegment(SDL_Renderer* renderer, const Segment* seg,
                              double halfWidth, double halfHeight,
                              Uint8 r, Uint8 g, Uint8 b) {
    SDL_Color light = { r, g, b, 255 };
    // Mismo factor de oscurecimiento que tenian los colores fijos originales
    // (140/214 ~= 0.65), asi que el degradado se ve igual de "tallado".
    SDL_Color dark  = { (Uint8)(r * 0.65f), (Uint8)(g * 0.65f), (Uint8)(b * 0.65f), 255 };
    fill_diamond_gradient(renderer, (int)seg->x, (int)seg->y,
                           halfWidth, halfHeight, seg->angle,
                           dark, light);
}

/* ===================================================================
 *  ENSAMBLAJE: la cabeza completa del dragon (coordenadas originales,
 *  SIN TOCAR -- esto es exactamente lo que ya te habia quedado bien)
 * =================================================================== */

/* --- CAPA DE PIEL: hocico, cabeza, nariz y orejas (todo lo que antes era
 * "rojo"). Se dibuja en tonos de GRIS/BLANCO puro (no en rojo) para que
 * SDL_SetTextureColorMod pueda teñirlo de verdad con cualquiera de los 4
 * colores del dragon: multiplicar un pixel blanco (255,255,255) por el
 * tinte (r,g,b) da exactamente (r,g,b); en cambio multiplicar un pixel ya
 * rojo (214,32,38) por un tinte azul solo puede oscurecer el rojo, nunca
 * volverlo azul. */
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
 * ensureHeadTextures(): cada variante usa los OTROS 3 colores de la
 * paleta que le sobran a ese color de piel, uno por anillo. Por eso esta
 * funcion recibe los 3 colores ya resueltos en vez de calcularlos. */
static void drawDragonHeadArtStars(SDL_Renderer* renderer,
                                    SDL_Color outer, SDL_Color mid, SDL_Color inner) {
    draw_star(renderer, 700, 300, 170, 140, 12, 0,  outer);
    draw_star(renderer, 700, 300, 110,  85,  8,  20, mid);
    draw_star(renderer, 700, 300, 85,  65,  8,  0, inner);
}

/* --- CAPA FIJA: ojo, dientes, fosa nasal, boca y cuerno. Estos SIEMPRE
 * se dibujan con su color real y nunca reciben SDL_SetTextureColorMod
 * (o se les aplica un mod neutro 255,255,255), por eso el negro y el
 * blanco no se alteran sin importar el color que le toque al dragon. */
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
 *  TEXTURA DE LA CABEZA: se dibuja una sola vez (normal y espejada) y
 *  despues se posiciona/rota/escala con SDL_RenderCopyEx cada frame.
 * =================================================================== */

// Tamano del "lienzo" donde vive el dibujo de la cabeza (coincide con el
// canvas original del prototipo, para no cortar nada).
#define HEAD_TEX_W 900
#define HEAD_TEX_H 650

// Punto donde el cuello se une al cuerpo, EN COORDENADAS ORIGINALES del
// dibujo (el centro del estallido de estrellas). Este es el punto que se
// hace coincidir con (x, y) al llamar renderDragonHead(), y tambien el
// pivote de la rotacion.
#define NECK_ANCHOR_X 700.0f
#define NECK_ANCHOR_Y 300.0f

// Capa "de piel" (hocico, cabeza, nariz, orejas): se tine con
// SDL_SetTextureColorMod segun el color de cada dragon.
static SDL_Texture* s_headTexNormalSkin = NULL;
static SDL_Texture* s_headTexMirrorSkin = NULL;
// Capa "de estrellas" (melena/estallido): 4 variantes horneadas, una por
// cada posible color de piel del dragon (indice 0..3 = DRAGON_PALETTE).
// Nunca se tine: cada variante ya trae los 3 colores reales que le
// sobran a ese color de piel.
static SDL_Texture* s_headTexNormalStars[DRAGON_PALETTE_SIZE] = { NULL };
static SDL_Texture* s_headTexMirrorStars[DRAGON_PALETTE_SIZE] = { NULL };
// Capa "fija" (ojo, dientes, boca, fosa nasal, cuerno): jamas se tine.
static SDL_Texture* s_headTexNormalFixed = NULL;
static SDL_Texture* s_headTexMirrorFixed = NULL;

static SDL_Texture* makeHeadLayerTexture(SDL_Renderer* renderer, void (*drawFn)(SDL_Renderer*)) {
    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET, HEAD_TEX_W, HEAD_TEX_H);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer, tex);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    drawFn(renderer);
    return tex;
}

// Igual que makeHeadLayerTexture, pero para la capa de estrellas, que
// necesita 3 colores concretos (no una funcion sin argumentos).
static SDL_Texture* makeStarsLayerTexture(SDL_Renderer* renderer,
                                           SDL_Color outer, SDL_Color mid, SDL_Color inner) {
    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET, HEAD_TEX_W, HEAD_TEX_H);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer, tex);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    drawDragonHeadArtStars(renderer, outer, mid, inner);
    return tex;
}

static SDL_Texture* makeMirroredTexture(SDL_Renderer* renderer, SDL_Texture* source) {
    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET, HEAD_TEX_W, HEAD_TEX_H);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(renderer, tex);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_Rect full = { 0, 0, HEAD_TEX_W, HEAD_TEX_H };
    SDL_RenderCopyEx(renderer, source, NULL, &full, 0.0, NULL, SDL_FLIP_HORIZONTAL);
    return tex;
}

static void ensureHeadTextures(SDL_Renderer* renderer) {
    if (s_headTexNormalSkin) return;

    SDL_Texture* prevTarget = SDL_GetRenderTarget(renderer);

    s_headTexNormalSkin  = makeHeadLayerTexture(renderer, drawDragonHeadArtSkin);
    s_headTexNormalFixed = makeHeadLayerTexture(renderer, drawDragonHeadArtFixed);
    s_headTexMirrorSkin  = makeMirroredTexture(renderer, s_headTexNormalSkin);
    s_headTexMirrorFixed = makeMirroredTexture(renderer, s_headTexNormalFixed);

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
        s_headTexNormalStars[skinIdx] = makeStarsLayerTexture(renderer,
                                             otherColors[0], otherColors[1], otherColors[2]);
        s_headTexMirrorStars[skinIdx] = makeMirroredTexture(renderer, s_headTexNormalStars[skinIdx]);
    }

    SDL_SetRenderTarget(renderer, prevTarget);
}

void renderDragonHead(SDL_Renderer* renderer, float x, float y, float scale, float angleDeg,
                       Uint8 r, Uint8 g, Uint8 b, int colorIndex) {
    ensureHeadTextures(renderer);

    // Normalizar el angulo a (-180, 180]
    float a = angleDeg;
    while (a > 180.0f)  a -= 360.0f;
    while (a <= -180.0f) a += 360.0f;

    // Convencion: 0 grados = mirando a la derecha, 180/-180 = izquierda
    // (igual que atan2(dy,dx) con Y hacia abajo). El dibujo original mira
    // a la izquierda, o sea "nace" en 180 grados.
    //
    // Para no rotar la cabeza 180 grados de golpe (lo que la dejaria de
    // cabeza), cuando el rumbo esta en la mitad derecha usamos la textura
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

    SDL_Texture* starsTex = facingRight ? s_headTexMirrorStars[colorIndex] : s_headTexNormalStars[colorIndex];
    SDL_Texture* skinTex  = facingRight ? s_headTexMirrorSkin : s_headTexNormalSkin;
    SDL_Texture* fixedTex = facingRight ? s_headTexMirrorFixed : s_headTexNormalFixed;

    // Solo la piel recibe el tinte del dragon (hocico, cabeza, nariz,
    // orejas). Las estrellas ya vienen "horneadas" con sus 3 colores
    // reales (los que le sobran a este color de piel) y la capa fija
    // (ojo, dientes, boca, cuerno) se deja en 255,255,255 = sin tinte,
    // para que negro y blanco no cambien.
    SDL_SetTextureColorMod(skinTex, r, g, b);
    SDL_SetTextureColorMod(starsTex, 255, 255, 255);
    SDL_SetTextureColorMod(fixedTex, 255, 255, 255);

    SDL_FRect dst;
    dst.w = HEAD_TEX_W * scale;
    dst.h = HEAD_TEX_H * scale;
    // Colocamos el rectangulo de forma que el punto de anclaje (antes de
    // rotar) caiga justo en (x, y)...
    dst.x = x - anchorLocalX * scale;
    dst.y = y - anchorLocalY * scale;

    // ...y pivoteamos la rotacion exactamente en ese mismo punto, para
    // que (x, y) -- donde arranca el cuerpo -- no se mueva al girar.
    SDL_FPoint center = { anchorLocalX * scale, anchorLocalY * scale };

    // Orden: estrellas al fondo, piel encima (tapa el centro del
    // estallido), y la capa fija (ojo/dientes/boca/cuerno) hasta arriba.
    SDL_RenderCopyExF(renderer, starsTex, NULL, &dst, artAngle, &center, SDL_FLIP_NONE);
    SDL_RenderCopyExF(renderer, skinTex,  NULL, &dst, artAngle, &center, SDL_FLIP_NONE);
    SDL_RenderCopyExF(renderer, fixedTex, NULL, &dst, artAngle, &center, SDL_FLIP_NONE);
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

void renderDragon(SDL_Renderer* renderer, const Dragon* dragon,
                    float headScale, float bodyHalfWidth, float bodyHalfHeight) {
    Uint8 r = (Uint8)dragon->r, g = (Uint8)dragon->g, b = (Uint8)dragon->b;
    float depthScale = 1.0f / dragon->depth;   // <-- nuevo

    for (int i = dragon->numSegments - 1; i >= 1; i--) {
        renderDragonBodySegment(renderer, &dragon->segments[i],
                                bodyHalfWidth * depthScale, bodyHalfHeight * depthScale, r,g,b);
    }

    const Segment* head = &dragon->segments[0];
    renderDragonHead(renderer, head->x, head->y, headScale * depthScale, head->angle, r, g, b, dragon->colorIndex);
}