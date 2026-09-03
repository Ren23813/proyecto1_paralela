#include <stdlib.h>
#include <math.h>
#include "firework.h"

//paramtros de las partículas de velocidad y tiempo
#define GRAVITY 60.0f               // aceleración hacia abajo de las particulas    
#define PARTICLE_MIN_SPEED 40.0f    // Velocidad mínima de dispersión de las partículas
#define PARTICLE_MAX_SPEED 120.0f   // Velocidad máxima de dispersión de las partículas
#define PARTICLE_LIFE_SECONDS 1.5f  // Duración en segundos de las partículas antes de desaparecer
#define RESPAWN_COOLDOWN 0.5f       // Tiempo de espera tras la extinción antes de reaparecer


// FUNCIONES AUXILIARES Y DE INICIALIZACIÓN

// Genera un número flotante aleatorio en el rango [lo, hi].
// Utiliza una sección crítica de OpenMP para evitar condiciones de carrera
// en 'rand()' al ser llamada por múltiples hilos en paralelo.
static float randRange(float lo, float hi) {
    float r;
    #pragma omp critical(rng_lock)
    {
        r = lo + (hi - lo) * ((float)rand() / (float)RAND_MAX);
    }
    return r;
}


// Reinicia un firework: nueva posición de lanzamiento, nuevo color, nuevo objetivo
// para el ciclo de vida completo de fuego artificial
static void resetFirework(Firework* fw, int windowW, int windowH) {
    fw->x = randRange(windowW * 0.15f, windowW * 0.85f);
    fw->y = (float)windowH;
    fw->targetY = randRange(windowH * 0.15f, windowH * 0.5f);
    fw->velY = randRange(150.0f, 220.0f);
    fw->state = FIREWORK_RISING;

    // color aleatorio vistoso (evitamos tonos muy oscuros para que resalten en el fondo)
    fw->r = randRange(120, 255);
    fw->g = randRange(120, 255);
    fw->b = randRange(120, 255);

    fw->cooldown = 0.0f;
}

// Inicializa el arreglo de fuegos artificiales y reserva su segmento de partículas.
// Mapea en el arreglo global de partículas un bloque contiguo para cada fuego artificial.
void initFireworks(Firework* fireworks, int numFireworks,
                    Particle* particles, int particlesPerFirework,
                    int windowW, int windowH) {
    for (int i = 0; i < numFireworks; i++) {
        // Delimita el bloque de partículas perteneciente a este fuego artificial
        fireworks[i].particleStart = i * particlesPerFirework;
        fireworks[i].particleCount = particlesPerFirework;

        // Todas las partículas de este firework empiezan inactivas.
        for (int p = 0; p < particlesPerFirework; p++) {
            particles[fireworks[i].particleStart + p].active = 0;
        }

        // Asigna parámetros base y profundidad en el plano (0.6 a 1.6)
        resetFirework(&fireworks[i], windowW, windowH);
        fireworks[i].depth = randRange(0.6f, 1.6f);

        // Para que no todos exploten al mismo tiempo al arrancar el programa,
        // se les da una posicion inicial de "subida" escalonada y aleatoria.
        fireworks[i].y = randRange(fireworks[i].targetY, (float)windowH);
    }
}

// Dispara las partículas de un firework en direcciones aleatorias (trigonometria).
// realiza la transicion de las explosiones 
static void explode(Firework* fw, Particle* particles) {
    for (int i = 0; i < fw->particleCount; i++) {
        Particle* p = &particles[fw->particleStart + i];
        float angle = randRange(0.0f, 2.0f * (float)M_PI);
        float speed = randRange(PARTICLE_MIN_SPEED, PARTICLE_MAX_SPEED);

        p->x = fw->x;
        p->y = fw->y;
        p->velX = speed * cosf(angle);
        p->velY = speed * sinf(angle);
        p->life = 1.0f;
        p->active = 1;
    }
}

// MÁQUINA DE ESTADOS Y FÍSICA (UPDATE)

//Actualiza el estado y la física del fuego artificial en función del tiempo transcurrido (dt).
void updateFirework(Firework* fw, Particle* particles, float dt,
                     int windowW, int windowH) {
    switch (fw->state) {
        //ESTADO 1: ASCENSO DEL PROYECTIL
        case FIREWORK_RISING:
            //// Movimiento hacia arriba (coordenadas Y decrecientes)
            fw->y -= fw->velY * dt;

            // Si alcanza o supera la altura objetivo, gatilla la explosión
            if (fw->y <= fw->targetY) {
                explode(fw, particles);
                fw->state = FIREWORK_EXPLODED;
            }
            break;
        //ESTADO 2: EXPANSION Y FÍSICA DE LAS PARTÍCULAS
        case FIREWORK_EXPLODED: {
            int anyAlive = 0; // Bandera para rastrear si aún queda alguna partícula activa
            for (int i = 0; i < fw->particleCount; i++) {
                Particle* p = &particles[fw->particleStart + i];
                if (!p->active) continue;

                p->velY += GRAVITY * dt;      // gravedad
                p->x += p->velX * dt;
                p->y += p->velY * dt;
                p->life -= dt / PARTICLE_LIFE_SECONDS; //Decaimiento proporcional de vida

                if (p->life <= 0.0f) {
                    p->active = 0;  // Desactiva la partícula consumida
                } else {
                    anyAlive = 1;  // Indica que la simulación de esta explosión debe continuar
                }
            }
            // Si todas las partículas expiraron, pasa al estado de tiempo de espera
            if (!anyAlive) {
                fw->state = FIREWORK_DEAD;
                fw->cooldown = RESPAWN_COOLDOWN;
            }
            break;
        }

        //ESTADO 3: TIEMPO DE ESPERA PREVIO A REAPARECER
        case FIREWORK_DEAD:
            fw->cooldown -= dt;
            if (fw->cooldown <= 0.0f) {
                resetFirework(fw, windowW, windowH);// Reinicia el ciclo completo
            }
            break;
    }
}

// RENDERIZADO POR BANDAS (FRAMEBUFFER)
// Antes usaba SDL2_gfxPrimitives (filledCircleRGBA / thickLineRGBA), que llaman a SDL_Renderer por dentro. 
// Ahora dibuja directo sobre el framebuffer, 
// cepta un rango de filas [yStart, yEnd) para permitir el renderizado paralelo multihilo.
// Aplica perspectiva a través de 'depthScale' para simular profundidad de campo
void renderFirework(FrameBuffer* fb, const Firework* fw, const Particle* particles,
                     int yStart, int yEnd) { // Escala inversa: mayor profundidad = menor tamaño
    float depthScale = 1.0f / fw->depth;

    // Dibuja el proyectil ascendente (cabeza y estela trazada)
    if (fw->state == FIREWORK_RISING) {
        // Cabeza del proyectil
        fbFillCircle(fb, (int)fw->x, (int)fw->y, (int)(3 * depthScale),
                     (Uint8)fw->r, (Uint8)fw->g, (Uint8)fw->b, 255, yStart, yEnd);

        // Estela o cola vertical
        int thickness = (int)(2 * depthScale);
        if (thickness < 1) thickness = 1;
        fbThickLine(fb, fw->x, fw->y, fw->x, fw->y + 12 * depthScale,
                    thickness, (Uint8)fw->r, (Uint8)fw->g, (Uint8)fw->b, 150, yStart, yEnd);
    }
    // Dibuja el conjunto de partículas en explosión con desvanecimiento
    else if (fw->state == FIREWORK_EXPLODED) {
        for (int i = 0; i < fw->particleCount; i++) {
            const Particle* p = &particles[fw->particleStart + i];
            if (!p->active) continue;

            // Transparencia dinámicamente ligada a la vida restante de la partícula
            Uint8 alpha = (Uint8)(p->life * 255);
            fbFillCircle(fb, (int)p->x, (int)p->y, (int)(2 * depthScale),
                         (Uint8)fw->r, (Uint8)fw->g, (Uint8)fw->b, alpha, yStart, yEnd);
        }
    }
}