#ifndef LANTERN_H
#define LANTERN_H

#include <SDL2/SDL.h>
#include "elements.h"
#include "framebuffer.h"

// Se llama una vez al inicio del programa: calcula la forma del farol.
//Debe invocarse una sola vez durante la fase de inicialización.
void buildLanternTemplate(void);

// se llama cada frame, por cada farol: actualiza su x,y según el tiempo transcurrido.
void updateLantern(Lantern* lantern, float elapsedTime);

// Dibuja un farol en el framebuffer dentro del rango de filas especificado.
//  Compatible con la renderización multihilo por división en bandas horizontales.
void renderLantern(FrameBuffer* fb, const Lantern* lantern, int yStart, int yEnd);

#endif