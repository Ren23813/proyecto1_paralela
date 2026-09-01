// src/dragon.h
#ifndef DRAGON_H
#define DRAGON_H

#include <SDL2/SDL.h>
#include "elements.h"

// r,g,b = color propio de este dragon (0-255). Se aplica como tinte sobre
// el arte original via SDL_SetTextureColorMod: partes ya oscuras (ojo negro)
// casi no cambian, partes claras (dientes blancos, estallido de fondo) si
// se notan tenidas del color del dragon.
void renderDragonHead(SDL_Renderer* renderer, float x, float y, float scale, float angleDeg,
                       Uint8 r, Uint8 g, Uint8 b,int colorIndex);

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

void renderDragon(SDL_Renderer* renderer, const Dragon* dragon,
                   float headScale, float bodyHalfWidth, float bodyHalfHeight);

#endif