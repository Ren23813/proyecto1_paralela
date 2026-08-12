#include <math.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include "lantern.h"

#define LANTERN_SCALE 0.12f
#define REF_X 250.0f
#define REF_Y 340.0f

#define BODY_KEY_COUNT 10
#define STEPS_PER_SEG  8
#define BODY_POINT_COUNT (BODY_KEY_COUNT * STEPS_PER_SEG)

#define RIB_COUNT 5
#define RIB_STEPS 12

#define TASSEL_COUNT 5
#define TASSEL_STEPS 10

typedef struct { float x, y; } Vec2;

// Plantilla global: misma forma para todos los faroles
static Vec2 bodyOutline[BODY_POINT_COUNT];
static Vec2 ribPoints[RIB_COUNT][RIB_STEPS + 1];
static Vec2 tasselPoints[TASSEL_COUNT][TASSEL_STEPS + 1];
static Vec2 topCap[4], botCap[4];
static Vec2 ringCenter, knotCenter, stringTop, stringBottom;
static float ringRadius, knotRadius;

static Vec2 localP(float x, float y) {
    Vec2 v;
    v.x = (x - REF_X) * LANTERN_SCALE;
    v.y = (y - REF_Y) * LANTERN_SCALE;
    return v;
}

static Vec2 catmullRom(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float t) {
    float t2 = t*t, t3 = t2*t;
    Vec2 r;
    r.x = 0.5f*((2*p1.x) + (-p0.x+p2.x)*t + (2*p0.x-5*p1.x+4*p2.x-p3.x)*t2 + (-p0.x+3*p1.x-3*p2.x+p3.x)*t3);
    r.y = 0.5f*((2*p1.y) + (-p0.y+p2.y)*t + (2*p0.y-5*p1.y+4*p2.y-p3.y)*t2 + (-p0.y+3*p1.y-3*p2.y+p3.y)*t3);
    return r;
}

static Vec2 bezierCubic(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float t) {
    float u = 1-t, b0=u*u*u, b1=3*u*u*t, b2=3*u*t*t, b3=t*t*t;
    Vec2 r;
    r.x = b0*p0.x + b1*p1.x + b2*p2.x + b3*p3.x;
    r.y = b0*p0.y + b1*p1.y + b2*p2.y + b3*p3.y;
    return r;
}

void buildLanternTemplate(void) {
    Vec2 bodyKey[BODY_KEY_COUNT] = {
        localP(182,132), localP(122,205), localP(103,325), localP(122,445),
        localP(182,515), localP(318,515), localP(378,445),
        localP(397,325), localP(378,205), localP(318,132)
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

    Vec2 tc[4] = { localP(205,92), localP(295,92), localP(318,132), localP(182,132) };
    Vec2 bc[4] = { localP(182,515), localP(318,515), localP(298,552), localP(202,552) };
    for (int i = 0; i < 4; i++) { topCap[i] = tc[i]; botCap[i] = bc[i]; }

    float ribTopX[] = {200,225,250,275,300};
    float ribMidX[] = {150,195,250,305,350};
    float ribBotX[] = {200,225,250,275,300};
    for (int i = 0; i < RIB_COUNT; i++) {
        Vec2 p0 = localP(ribTopX[i], 140), p1 = localP(ribMidX[i], 230);
        Vec2 p2 = localP(ribMidX[i], 415), p3 = localP(ribBotX[i], 505);
        for (int s = 0; s <= RIB_STEPS; s++)
            ribPoints[i][s] = bezierCubic(p0, p1, p2, p3, (float)s / RIB_STEPS);
    }

    ringCenter = localP(250,58);   ringRadius = 14 * LANTERN_SCALE;
    knotCenter = localP(250,568);  knotRadius = 12 * LANTERN_SCALE;
    stringTop = localP(250,10);    stringBottom = localP(250,92);

    float tasselX[] = {215,233,250,267,285};
    for (int i = 0; i < TASSEL_COUNT; i++) {
        float dx = tasselX[i] - 250;
        Vec2 p0 = localP(250,572), p1 = localP(250+dx*0.6f, 610);
        Vec2 p2 = localP(tasselX[i], 640), p3 = localP(tasselX[i], 675);
        for (int s = 0; s <= TASSEL_STEPS; s++)
            tasselPoints[i][s] = bezierCubic(p0, p1, p2, p3, (float)s / TASSEL_STEPS);
    }
}

void updateLantern(Lantern* lantern, float elapsedTime) {
    lantern->x = lantern->baseX + lantern->amplitudeX * sinf(lantern->frequency * elapsedTime + lantern->phase);
    lantern->y = lantern->baseY + lantern->amplitudeY * cosf(lantern->frequency * elapsedTime + lantern->phase)
                 - lantern->driftY * elapsedTime;
}

void renderLantern(SDL_Renderer* renderer, const Lantern* lantern) {
    float cx = lantern->x, cy = lantern->y;

    Sint16 vx[BODY_POINT_COUNT], vy[BODY_POINT_COUNT];
    for (int i = 0; i < BODY_POINT_COUNT; i++) {
        vx[i] = (Sint16)(cx + bodyOutline[i].x);
        vy[i] = (Sint16)(cy + bodyOutline[i].y);
    }
    filledPolygonRGBA(renderer, vx, vy, BODY_POINT_COUNT, 199, 32, 38, 255);

    Sint16 tcx[4], tcy[4], bcx[4], bcy[4];
    for (int i = 0; i < 4; i++) {
        tcx[i] = (Sint16)(cx + topCap[i].x); tcy[i] = (Sint16)(cy + topCap[i].y);
        bcx[i] = (Sint16)(cx + botCap[i].x); bcy[i] = (Sint16)(cy + botCap[i].y);
    }
    filledPolygonRGBA(renderer, tcx, tcy, 4, 226, 171, 60, 255);
    filledPolygonRGBA(renderer, bcx, bcy, 4, 226, 171, 60, 255);

    for (int r = 0; r < RIB_COUNT; r++)
        for (int s = 0; s < RIB_STEPS; s++)
            thickLineRGBA(renderer,
                (Sint16)(cx+ribPoints[r][s].x),   (Sint16)(cy+ribPoints[r][s].y),
                (Sint16)(cx+ribPoints[r][s+1].x), (Sint16)(cy+ribPoints[r][s+1].y),
                2, 150, 18, 20, 255);

    thickLineRGBA(renderer, (Sint16)(cx+stringTop.x), (Sint16)(cy+stringTop.y),
                             (Sint16)(cx+stringBottom.x), (Sint16)(cy+stringBottom.y),
                             2, 176, 120, 28, 255);
    circleRGBA(renderer, (Sint16)(cx+ringCenter.x), (Sint16)(cy+ringCenter.y),
               (Sint16)ringRadius, 176, 120, 28, 255);

    filledCircleRGBA(renderer, (Sint16)(cx+knotCenter.x), (Sint16)(cy+knotCenter.y),
                      (Sint16)knotRadius, 176, 120, 28, 255);

    for (int t = 0; t < TASSEL_COUNT; t++)
        for (int s = 0; s < TASSEL_STEPS; s++)
            thickLineRGBA(renderer,
                (Sint16)(cx+tasselPoints[t][s].x),   (Sint16)(cy+tasselPoints[t][s].y),
                (Sint16)(cx+tasselPoints[t][s+1].x), (Sint16)(cy+tasselPoints[t][s+1].y),
                1, 226, 171, 60, 255);
}
