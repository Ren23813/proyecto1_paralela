// src/dragon.c
//
// Cabeza de dragon: una funcion por cada tipo de figura (circulo/ovalo,
// rectangulo redondeado, gota, estrella, linea ondulada, palito con rama)
// y renderDragonHead() que las combina. Todo esto viene del prototipo
// dragon_sdl.c (que dibujaba sobre una superficie en memoria y exportaba
// PNG); aqui se dibuja directo sobre el SDL_Renderer de la ventana.

#include <math.h>
#include <stdlib.h>
#include "dragon.h"

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
 * Independiente de la cabeza: no se llama desde renderDragonHead. */
static void fill_diamond_gradient(SDL_Renderer* ren, int cx, int cy,
                                   double halfWidth, double halfHeight,
                                   double angle_deg,
                                   SDL_Color colorLeft, SDL_Color colorRight) {
    double rad = angle_deg * M_PI / 180.0;
    double cos_a = cos(rad),  sin_a = sin(rad);
    double cos_ai = cos(-rad), sin_ai = sin(-rad); // rotacion inversa (para el degradado)

    // 4 vertices del rombo en espacio local: arriba, derecha, abajo, izquierda
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
            // este pixel, pasado a espacio local, me dice donde cae en el degradado
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

/* ===================================================================
 *  ENSAMBLAJE: la cabeza completa del dragon
 * =================================================================== */
static const SDL_Color BODY_DARK_RED = { 140, 20, 22, 255 };
static const SDL_Color BODY_RED      = { 214, 32, 38, 255 };

void renderDragonBodySegment(SDL_Renderer* renderer, const Segment* seg,
                              double halfWidth, double halfHeight) {
    fill_diamond_gradient(renderer, (int)seg->x, (int)seg->y,
                           halfWidth, halfHeight, seg->angle,
                           BODY_DARK_RED, BODY_RED);
}

void renderDragonHead(SDL_Renderer* renderer) {
    SDL_Color red      = { 214, 32, 38, 255 };
    SDL_Color darkRed  = { 150, 18, 20, 255 };
    SDL_Color black    = { 20, 18, 16, 255 };
    SDL_Color white    = { 245, 245, 240, 255 };
    SDL_Color brown    = { 120, 72, 24, 255 };
    SDL_Color blue     = { 60, 92, 150, 255 };
    SDL_Color green    = { 142, 181, 62, 255 };
    SDL_Color yellow   = { 231, 190, 40, 255 };

    /* --- Estallido / estrellas detras de la cabeza --- */
    draw_star(renderer, 700, 300, 170, 140, 12, 0,  blue);
    draw_star(renderer, 700, 300, 110,  85,  8,  20, green);
    draw_star(renderer, 700, 300, 85,  65,  8,  0, yellow);

    /* --- Cuerpo principal: hocico + cabeza (rectangulos redondeados) --- */
    fill_rounded_rect(renderer, 220,  275, 400, 125, 30, red);   /* hocico alargado */
     /* bloque de la cabeza */
    fill_rounded_rect(renderer, 500, 200, 175, 200, 30, red);  
    /*boca*/
    fill_rounded_rect(renderer, 220,  320, 280, 50, 20, white); 

    /* --- Dientes en zigzag a lo largo de la boca --- */
    draw_wavy_line(renderer, 220, 350, 500, 350, 13, 50, 2, black);
    /*nariz roja*/
    fill_ellipse(renderer, 265, 275, 38, 50, 50, red);

    /*fosa nasal*/
    fill_ellipse(renderer, 275, 285, 10, 16,40,  black);
    /* --- Ojol --- */
    fill_ellipse(renderer, 580, 270, 20, 35,70, black);

    /* ---oreja --- */
    draw_teardrop(renderer, 680, 220, 55, 50, red);
    /* --- interior oreja --- */
    draw_teardrop(renderer, 690, 220, 30, 50, darkRed);
    /* --- Cuernito tipo ramita en la parte de arriba de la cabeza --- */
    draw_stick(renderer, 640, 208, -110, 70, 8, brown);
}