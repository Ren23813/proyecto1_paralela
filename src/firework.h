
#ifndef FIREWORK_H
#define FIREWORK_H

#include <SDL2/SDL.h>
#include "elements.h"

// Inicializa N fireworks y reserva sus rangos dentro del pool de particulas.
// particlesPerFirework define cuantas partículas genera cada explosion.
void initFireworks(Firework* fireworks, int numFireworks,
                    Particle* particles, int particlesPerFirework,
                    int windowW, int windowH);

// Avanza la simulacion un paso de tiempo (dt en segundos).
void updateFirework(Firework* fw, Particle* particles, float dt,
                     int windowW, int windowH);

void renderFirework(SDL_Renderer* renderer, const Firework* fw, const Particle* particles);

#endif
