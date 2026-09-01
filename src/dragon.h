// src/dragon.h
#ifndef DRAGON_H
#define DRAGON_H

#include <SDL2/SDL.h>
#include "elements.h"

// Numero maximo de "capas de render" (renderers independientes) que puede
// usar la version paralela. Cada capa mantiene su PROPIO cache de texturas
// de cabeza (ver dragon.c), porque las texturas SDL estan atadas al
// SDL_Renderer que las creo y no se pueden compartir entre renderers.
#define DRAGON_MAX_RENDER_LAYERS 16

// r,g,b = color propio de este dragon (0-255). Se aplica como tinte sobre
// el arte original via SDL_SetTextureColorMod: partes ya oscuras (ojo negro)
// casi no cambian, partes claras (dientes blancos, estallido de fondo) si
// se notan tenidas del color del dragon.
//
// layerIndex identifica CUAL renderer (capa) esta llamando: el cache de
// texturas de la cabeza se guarda por separado para cada layerIndex, para
// que nunca se use una textura creada por un renderer en OTRO renderer
// (eso es undefined behavior en SDL). Debe estar en [0, DRAGON_MAX_RENDER_LAYERS).
void renderDragonHead(SDL_Renderer* renderer, float x, float y, float scale, float angleDeg,
                       Uint8 r, Uint8 g, Uint8 b, int colorIndex, int layerIndex);

// r,g,b = color "claro" del degradado del rombo; el lado oscuro se calcula
// internamente como una version atenuada del mismo color.
void renderDragonBodySegment(SDL_Renderer* renderer, const Segment* seg,
                              double halfWidth, double halfHeight,
                              Uint8 r, Uint8 g, Uint8 b);

void initDragon(Dragon* dragon, int numSegments, float segmentSpacing,
                 float startX, float startY, float speed,
                 int windowW, int windowH);

void freeDragon(Dragon* dragon);

void updateDragon(Dragon* dragon, float dt, int windowW, int windowH);

// layerIndex: mismo significado que en renderDragonHead (que capa/renderer
// esta dibujando). El cuerpo (renderDragonBodySegment) no usa texturas
// cacheadas, asi que es seguro en cualquier renderer sin importar layerIndex.
void renderDragon(SDL_Renderer* renderer, const Dragon* dragon,
                   float headScale, float bodyHalfWidth, float bodyHalfHeight,
                   int layerIndex);

#endif