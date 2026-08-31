// src/dragon.h
#ifndef DRAGON_H
#define DRAGON_H

#include <SDL2/SDL.h>
#include "elements.h"

// Dibuja la cabeza del dragon en coordenadas fijas del canvas (900x650),
// tal como esta en el prototipo. Todavia no usa offsetX/offsetY ni la
// struct Dragon (segments) -- eso llega cuando armemos el cuerpo completo
// y la animacion. Por ahora es solo para poder ver y ajustar las piezas.
void renderDragonHead(SDL_Renderer* renderer);

#endif