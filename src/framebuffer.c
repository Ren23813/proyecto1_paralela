// src/framebuffer.c
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include "framebuffer.h"

FrameBuffer* fbCreate(int width, int height) {
    FrameBuffer* fb = malloc(sizeof(FrameBuffer));
    if (!fb) return NULL;
    fb->pixels = malloc(sizeof(Uint32) * (size_t)width * (size_t)height);
    if (!fb->pixels) { free(fb); return NULL; }
    fb->width = width;
    fb->height = height;
    return fb;
}

void fbDestroy(FrameBuffer* fb) {
    if (!fb) return;
    free(fb->pixels);
    free(fb);
}

void fbClearRows(FrameBuffer* fb, Uint8 r, Uint8 g, Uint8 b, int yStart, int yEnd) {
    if (yStart < 0) yStart = 0;
    if (yEnd > fb->height) yEnd = fb->height;
    Uint32 val = fbPackRGBA8888(r, g, b, 255);
    for (int y = yStart; y < yEnd; y++) {
        Uint32* row = fb->pixels + (size_t)y * fb->width;
        for (int x = 0; x < fb->width; x++) row[x] = val;
    }
}

void fbBlendPixel(FrameBuffer* fb, int x, int y, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    if ((unsigned)x >= (unsigned)fb->width || (unsigned)y >= (unsigned)fb->height) return;
    if (a == 0) return;
    Uint32* p = fb->pixels + (size_t)y * fb->width + x;
    if (a >= 255) { *p = fbPackRGBA8888(r, g, b, 255); return; }
    Uint8 dr, dg, db, da;
    fbUnpackRGBA8888(*p, &dr, &dg, &db, &da);
    int ia = (int)a;
    Uint8 outR = (Uint8)((r * ia + dr * (255 - ia)) / 255);
    Uint8 outG = (Uint8)((g * ia + dg * (255 - ia)) / 255);
    Uint8 outB = (Uint8)((b * ia + db * (255 - ia)) / 255);
    *p = fbPackRGBA8888(outR, outG, outB, 255);
}

void fbFillCircle(FrameBuffer* fb, int cx, int cy, int radius,
                   Uint8 r, Uint8 g, Uint8 b, Uint8 a, int yStart, int yEnd) {
    if (radius < 1) radius = 1;
    int y0 = cy - radius; if (y0 < yStart) y0 = yStart;
    int y1 = cy + radius; if (y1 > yEnd - 1) y1 = yEnd - 1;
    for (int y = y0; y <= y1; y++) {
        int dy = y - cy;
        int limitSq = radius * radius - dy * dy;
        if (limitSq < 0) continue;
        int dxLimit = (int)sqrt((double)limitSq);
        for (int x = cx - dxLimit; x <= cx + dxLimit; x++) {
            fbBlendPixel(fb, x, y, r, g, b, a);
        }
    }
}

void fbThickLine(FrameBuffer* fb, double x0, double y0, double x1, double y1,
                  int thickness, Uint8 r, Uint8 g, Uint8 b, Uint8 a, int yStart, int yEnd) {
    double dist = hypot(x1 - x0, y1 - y0);
    int steps = (int)dist + 1;
    int rad = thickness / 2;
    if (rad < 1) rad = 1;
    for (int s = 0; s <= steps; s++) {
        double t = (double)s / steps;
        int px = (int)(x0 + (x1 - x0) * t);
        int py = (int)(y0 + (y1 - y0) * t);
        fbFillCircle(fb, px, py, rad, r, g, b, a, yStart, yEnd);
    }
}

// Algoritmo del punto medio (midpoint circle), variante con err = 1 - radius.
void fbCircleOutline(FrameBuffer* fb, int cx, int cy, int radius,
                      Uint8 r, Uint8 g, Uint8 b, Uint8 a, int yStart, int yEnd) {
    int x = radius, y = 0, err = 1 - radius;
    while (x >= y) {
        int cand[8][2] = {
            { cx + x, cy + y }, { cx + y, cy + x }, { cx - y, cy + x }, { cx - x, cy + y },
            { cx - x, cy - y }, { cx - y, cy - x }, { cx + y, cy - x }, { cx + x, cy - y }
        };
        for (int i = 0; i < 8; i++) {
            int py = cand[i][1];
            if (py >= yStart && py < yEnd) fbBlendPixel(fb, cand[i][0], py, r, g, b, a);
        }
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void fbFillPolygon(FrameBuffer* fb, const SDL_Point* pts, int n,
                    Uint8 r, Uint8 g, Uint8 b, Uint8 a, int yStart, int yEnd) {
    int miny = pts[0].y, maxy = pts[0].y;
    for (int i = 1; i < n; i++) {
        if (pts[i].y < miny) miny = pts[i].y;
        if (pts[i].y > maxy) maxy = pts[i].y;
    }
    if (miny < yStart) miny = yStart;
    if (maxy > yEnd - 1) maxy = yEnd - 1;
    if (miny > maxy) return;

    double* xs = malloc(sizeof(double) * n);
    for (int y = miny; y <= maxy; y++) {
        double yc = y + 0.5;
        int count = 0;
        for (int i = 0; i < n; i++) {
            SDL_Point pa = pts[i], pb = pts[(i + 1) % n];
            if ((pa.y <= yc && pb.y > yc) || (pb.y <= yc && pa.y > yc)) {
                double t = (yc - pa.y) / (double)(pb.y - pa.y);
                xs[count++] = pa.x + t * (pb.x - pa.x);
            }
        }
        for (int i = 1; i < count; i++) {
            double key = xs[i]; int j = i - 1;
            while (j >= 0 && xs[j] > key) { xs[j + 1] = xs[j]; j--; }
            xs[j + 1] = key;
        }
        for (int i = 0; i + 1 < count; i += 2) {
            int xa = (int)ceil(xs[i] - 0.5), xb = (int)floor(xs[i + 1] - 0.5);
            for (int x = xa; x <= xb; x++) fbBlendPixel(fb, x, y, r, g, b, a);
        }
    }
    free(xs);
}

void fbFillDiamondGradient(FrameBuffer* fb, int cx, int cy,
                            double halfWidth, double halfHeight, double angle_deg,
                            Uint8 lr, Uint8 lg, Uint8 lb,
                            Uint8 rr, Uint8 rg, Uint8 rb,
                            int yStart, int yEnd) {
    double rad = angle_deg * M_PI / 180.0;
    double cos_a = cos(rad), sin_a = sin(rad);
    double cos_ai = cos(-rad), sin_ai = sin(-rad);

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
    if (miny < yStart) miny = yStart;
    if (maxy > yEnd - 1) maxy = yEnd - 1;
    if (miny > maxy) return;

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

            Uint8 pr = (Uint8)(lr + (rr - lr) * f);
            Uint8 pg = (Uint8)(lg + (rg - lg) * f);
            Uint8 pb = (Uint8)(lb + (rb - lb) * f);
            fbSetPixel(fb, x, y, pr, pg, pb, 255);
        }
    }
}