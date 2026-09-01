#include <stdlib.h>
#include <math.h>
#include "firework.h"

#define GRAVITY 60.0f          // aceleración hacia abajo de las particulas
#define PARTICLE_MIN_SPEED 40.0f
#define PARTICLE_MAX_SPEED 120.0f
#define PARTICLE_LIFE_SECONDS 1.5f
#define RESPAWN_COOLDOWN 0.5f

// aleatorio flotante entre lo y hi
static float randRange(float lo, float hi) {
    float r;
    #pragma omp critical(rng_lock)
    {
        r = lo + (hi - lo) * ((float)rand() / (float)RAND_MAX);
    }
    return r;
}

// Reinicia un firework: nueva posición de lanzamiento, nuevo color, nuevo objetivo.
static void resetFirework(Firework* fw, int windowW, int windowH) {
    fw->x = randRange(windowW * 0.15f, windowW * 0.85f);
    fw->y = (float)windowH;
    fw->targetY = randRange(windowH * 0.15f, windowH * 0.5f);
    fw->velY = randRange(150.0f, 220.0f);
    fw->state = FIREWORK_RISING;

    // color aleatorio vistoso (evitamos tonos muy oscuros)
    fw->r = randRange(120, 255);
    fw->g = randRange(120, 255);
    fw->b = randRange(120, 255);

    fw->cooldown = 0.0f;
}

void initFireworks(Firework* fireworks, int numFireworks,
                    Particle* particles, int particlesPerFirework,
                    int windowW, int windowH) {
    for (int i = 0; i < numFireworks; i++) {
        fireworks[i].particleStart = i * particlesPerFirework;
        fireworks[i].particleCount = particlesPerFirework;

        // Todas las partículas de este firework arrancan inactivas.
        for (int p = 0; p < particlesPerFirework; p++) {
            particles[fireworks[i].particleStart + p].active = 0;
        }

        resetFirework(&fireworks[i], windowW, windowH);
        fireworks[i].depth = randRange(0.6f, 1.6f);

        // Para que no todos exploten al mismo tiempo al arrancar el programa,
        // les damos una posicion inicial de "subida" escalonada y aleatoria.
        fireworks[i].y = randRange(fireworks[i].targetY, (float)windowH);
    }
}

// Dispara las partículas de un firework en direcciones aleatorias (trigonometria).
static void explode(Firework* fw, Particle* particles) {
    for (int i = 0; i < fw->particleCount; i++) {
        Particle* p = &particles[fw->particleStart + i];
        float angle = randRange(0.0f, 2.0f * (float)M_PI);
        float speed = randRange(PARTICLE_MIN_SPEED, PARTICLE_MAX_SPEED);

        p->x = fw->x;
        p->y = fw->y;
        p->velX = speed * cosf(angle);
        p->velY = speed * sinf(angle);
        p->life = 1.0f;
        p->active = 1;
    }
}

void updateFirework(Firework* fw, Particle* particles, float dt,
                     int windowW, int windowH) {
    switch (fw->state) {

        case FIREWORK_RISING:
            fw->y -= fw->velY * dt;
            if (fw->y <= fw->targetY) {
                explode(fw, particles);
                fw->state = FIREWORK_EXPLODED;
            }
            break;

        case FIREWORK_EXPLODED: {
            int anyAlive = 0;
            for (int i = 0; i < fw->particleCount; i++) {
                Particle* p = &particles[fw->particleStart + i];
                if (!p->active) continue;

                p->velY += GRAVITY * dt;      // gravedad
                p->x += p->velX * dt;
                p->y += p->velY * dt;
                p->life -= dt / PARTICLE_LIFE_SECONDS;

                if (p->life <= 0.0f) {
                    p->active = 0;
                } else {
                    anyAlive = 1;
                }
            }
            if (!anyAlive) {
                fw->state = FIREWORK_DEAD;
                fw->cooldown = RESPAWN_COOLDOWN;
            }
            break;
        }

        case FIREWORK_DEAD:
            fw->cooldown -= dt;
            if (fw->cooldown <= 0.0f) {
                resetFirework(fw, windowW, windowH);
            }
            break;
    }
}

// Antes usaba SDL2_gfxPrimitives (filledCircleRGBA / thickLineRGBA), que
// llaman a SDL_Renderer por dentro. Ahora dibuja directo sobre el
// framebuffer, recortado a la banda [yStart, yEnd), asi se puede llamar en
// paralelo por bandas junto con dragones y faroles.
void renderFirework(FrameBuffer* fb, const Firework* fw, const Particle* particles,
                     int yStart, int yEnd) {
    float depthScale = 1.0f / fw->depth;
    if (fw->state == FIREWORK_RISING) {
        fbFillCircle(fb, (int)fw->x, (int)fw->y, (int)(3 * depthScale),
                     (Uint8)fw->r, (Uint8)fw->g, (Uint8)fw->b, 255, yStart, yEnd);

        int thickness = (int)(2 * depthScale);
        if (thickness < 1) thickness = 1;
        fbThickLine(fb, fw->x, fw->y, fw->x, fw->y + 12 * depthScale,
                    thickness, (Uint8)fw->r, (Uint8)fw->g, (Uint8)fw->b, 150, yStart, yEnd);
    }
    else if (fw->state == FIREWORK_EXPLODED) {
        for (int i = 0; i < fw->particleCount; i++) {
            const Particle* p = &particles[fw->particleStart + i];
            if (!p->active) continue;
            Uint8 alpha = (Uint8)(p->life * 255);
            fbFillCircle(fb, (int)p->x, (int)p->y, (int)(2 * depthScale),
                         (Uint8)fw->r, (Uint8)fw->g, (Uint8)fw->b, alpha, yStart, yEnd);
        }
    }
}