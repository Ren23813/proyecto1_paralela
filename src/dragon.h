// src/dragon.h
#ifndef DRAGON_H
#define DRAGON_H

#include <SDL2/SDL.h>
#include "elements.h"
#include "framebuffer.h"

// Genera (UNA sola vez, al arranque) el arte de la cabeza como buffers de
// pixeles en RAM en vez de texturas SDL 
void initDragonHeadArt(SDL_Renderer* renderer);

// r,g,b = color "claro" del degradado del rombo; el lado oscuro se calcula
// internamente como una version atenuada del mismo color. Escribe directo
// en el framebuffer, recortado a la banda de filas [yStart, yEnd).
void renderDragonBodySegment(FrameBuffer* fb, const Segment* seg,
                              double halfWidth, double halfHeight,
                              Uint8 r, Uint8 g, Uint8 b, int yStart, int yEnd);

void initDragon(Dragon* dragon, int numSegments, float segmentSpacing,
                 float startX, float startY, float speed,
                 int windowW, int windowH);

void freeDragon(Dragon* dragon);

void updateDragon(Dragon* dragon, float dt, int windowW, int windowH);

// Dibuja el dragon COMPLETO (todo el cuerpo + la cabeza) directo sobre el
// framebuffer, recortado a la banda [yStart, yEnd). Al no depender de
// SDL_Renderer para nada, se puede llamar desde varios hilos a la vez
// (uno por banda) y, ademas, se puede intercalar en el mismo orden de
// profundidad que fuegos artificiales y faroles 
void renderDragon(FrameBuffer* fb, const Dragon* dragon,
                   double headScale, double bodyHalfWidth, double bodyHalfHeight,
                   int yStart, int yEnd);

#endif