// src/dragon.h
#ifndef DRAGON_H
#define DRAGON_H

#include <SDL2/SDL.h>
#include "elements.h"
#include "framebuffer.h"

// r,g,b = color propio de este dragon (0-255). Se aplica como tinte sobre
// el arte original via SDL_SetTextureColorMod: partes ya oscuras (ojo negro)
// casi no cambian, partes claras (dientes blancos, estallido de fondo) si
// se notan tenidas del color del dragon.
//
// La cabeza sigue siendo una textura pre-generada (ensureHeadTextures) que
// se posiciona/rota con SDL_RenderCopyExF: eso ya es barato, no hace falta
// tocarlo. Por eso sigue recibiendo un SDL_Renderer* y se dibuja aparte,
// DESPUES de subir el framebuffer a pantalla (ver main).
void renderDragonHead(SDL_Renderer* renderer, float x, float y, float scale, float angleDeg,
                       Uint8 r, Uint8 g, Uint8 b,int colorIndex);

// r,g,b = color "claro" del degradado del rombo; el lado oscuro se calcula
// internamente como una version atenuada del mismo color.
//
// El cuerpo, en cambio, se dibuja directo sobre el FrameBuffer (nunca
// llama funciones de SDL_Renderer), recortado a la banda de filas
// [yStart, yEnd) -- asi se puede llamar en paralelo desde varios hilos
// siempre que cada uno tenga su propia banda, sin ningun lock.
void renderDragonBodySegment(FrameBuffer* fb, const Segment* seg,
                              double halfWidth, double halfHeight,
                              Uint8 r, Uint8 g, Uint8 b, int yStart, int yEnd);

void initDragon(Dragon* dragon, int numSegments, float segmentSpacing,
                 float startX, float startY, float speed,
                 int windowW, int windowH);

void freeDragon(Dragon* dragon);

void updateDragon(Dragon* dragon, float dt, int windowW, int windowH);

// Dibuja SOLO el cuerpo (todos los rombos) de un dragon en el framebuffer,
// recortado a la banda [yStart, yEnd). La cabeza se dibuja aparte con
// renderDragonHead.
void renderDragonBody(FrameBuffer* fb, const Dragon* dragon,
                       double bodyHalfWidth, double bodyHalfHeight, int yStart, int yEnd);

#endif