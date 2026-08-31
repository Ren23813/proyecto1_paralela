// src/dragon.h
// src/dragon.h
#ifndef DRAGON_H
#define DRAGON_H

#include <SDL2/SDL.h>
#include "elements.h"

// ---------------------------------------------------------------------
// CABEZA (pieza independiente, reutilizable)
// ---------------------------------------------------------------------
// Dibuja la cabeza del dragon en (x, y) -- el punto donde el cuello se
// une al cuerpo -- rotada 'angleDeg' grados (0 = mirando a la derecha,
// 180/-180 = mirando a la izquierda; mismo sistema que atan2(dy,dx) con
// Y creciendo hacia abajo, como en SDL) y escalada por 'scale' (1.0 =
// tamano original del prototipo, que es bastante grande: probar con
// valores chicos, ej. 0.15-0.25, para que quede proporcional al cuerpo).
//
// El espejado ante giros a la derecha es automatico e interno: no hace
// falta pasar ningun flag aparte, alcanza con angleDeg.
void renderDragonHead(SDL_Renderer* renderer, float x, float y, float scale, float angleDeg);

// Dibuja un segmento del cuerpo (rombo con degradado horizontal en su
// propio espacio local, rotado segun seg->angle).
void renderDragonBodySegment(SDL_Renderer* renderer, const Segment* seg,
                              double halfWidth, double halfHeight);

// ---------------------------------------------------------------------
// DRAGON COMPLETO (cabeza + cuerpo + animacion de "vagabundeo")
// ---------------------------------------------------------------------
// Reserva los segmentos (segments[0] = cabeza) y elige un primer destino
// al azar. segmentSpacing es la distancia en pixeles entre el centro de
// un segmento y el siguiente (usalo mas o menos igual al ancho del rombo
// del cuerpo para que no queden ni pegados ni separados).
void initDragon(Dragon* dragon, int numSegments, float segmentSpacing,
                 float startX, float startY, float speed,
                 int windowW, int windowH);

void freeDragon(Dragon* dragon);

// Mueve la cabeza en linea recta hacia dragon->targetX/targetY. Cuando
// llega cerca del destino o se acerca al borde de la pantalla, elige un
// nuevo destino al azar. Despues, cada segmento del cuerpo "hereda" la
// posicion y angulo que tenia el segmento de enfrente ANTES de moverse
// (efecto de serpiente siguiendo a la cabeza).
void updateDragon(Dragon* dragon, float dt, int windowW, int windowH);

// Dibuja el dragon completo: cuerpo primero (para que la cabeza tape la
// union con el primer segmento) y despues la cabeza.
void renderDragon(SDL_Renderer* renderer, const Dragon* dragon,
                   float headScale, float bodyHalfWidth, float bodyHalfHeight);

#endif