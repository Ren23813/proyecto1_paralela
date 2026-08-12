// src/elements.h
#ifndef ELEMENTS_H
#define ELEMENTS_H

//  DRAGON 
// Un segmento del cuerpo (segments[0] = cabeza, resto = cuerpo)
typedef struct {
    float x, y;
    float angle;   // orientacion, para rotar el rombo al dibujarlo
} Segment;

typedef struct {
    Segment* segments;       // arreglo dinamico de segmentos
    int numSegments;
    float targetX, targetY;  // destino actual hacia donde vuela la cabeza
    float speed;
    float r, g, b;             // color propio de este dragon
} Dragon;

// FIREWORK  
typedef enum {
    FIREWORK_RISING,
    FIREWORK_EXPLODED,
    FIREWORK_DEAD
} FireworkState;

typedef struct {
    float x, y;
    float velY;
    FireworkState state;
    float r, g, b;
    int   particleStart;   // indice inicial en el arreglo global de particulas
    int   particleCount;   // cuantas particulas le pertenecen
} Firework;


// Pool global de particulas (compartido entre todas las explosiones de los Fireworks)
typedef struct {
    float x, y;
    float velX, velY;
    float life;      // tiempo de vida restante
    int   active;      // 0 = inactiva, 1 = activa
} Particle;

// LANTERN 
typedef struct {
    float baseX, baseY;
    float amplitudeX, amplitudeY;
    float frequency;
    float phase;
    float driftY;
    float x, y;
} Lantern;

#endif