#ifndef LANTERN_H
#define LANTERN_H

#include <SDL2/SDL.h>
#include "elements.h"

// Se llama una vez al inicio del programa: calcula la forma del farol.
void buildLanternTemplate(void);

// se llama cada frame, por cada farol: actualiza su x,y según el tiempo transcurrido.
void updateLantern(Lantern* lantern, float elapsedTime);

// Se llama cada frame, por cada farol: lo dibuja en su posicion actual (lantern->x, lantern->y).
void renderLantern(SDL_Renderer* renderer, const Lantern* lantern);

#endif