#ifndef FIREWORK_H
#define FIREWORK_H

#include <SDL2/SDL.h>
#include "elements.h"
#include "framebuffer.h"

// Inicializa N fireworks y reserva sus rangos dentro del pool de particulas.
// particlesPerFirework define cuantas partículas genera cada explosion.
void initFireworks(Firework* fireworks, int numFireworks,
                    Particle* particles, int particlesPerFirework,
                    int windowW, int windowH);

// Avanza la simulacion un paso de tiempo (dt en segundos).
void updateFirework(Firework* fw, Particle* particles, float dt,
                     int windowW, int windowH);

// Dibuja el firework directo sobre el framebuffer, recortado a la banda de
// filas [yStart, yEnd) -- para poder llamarlo en paralelo por bandas.
void renderFirework(FrameBuffer* fb, const Firework* fw, const Particle* particles,
                     int yStart, int yEnd);

#endif