#ifndef LANTERN_H
#define LANTERN_H

#include <SDL2/SDL.h>
#include "elements.h"
#include "framebuffer.h"

// Se llama una vez al inicio del programa: calcula la forma del farol.
void buildLanternTemplate(void);

// se llama cada frame, por cada farol: actualiza su x,y según el tiempo transcurrido.
void updateLantern(Lantern* lantern, float elapsedTime);

// Se llama cada frame, por cada farol: lo dibuja en su posicion actual
// (lantern->x, lantern->y), directo sobre el framebuffer y recortado a la
// banda de filas [yStart, yEnd) -- para poder llamarlo en paralelo.
void renderLantern(FrameBuffer* fb, const Lantern* lantern, int yStart, int yEnd);

#endif