#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "elements.h"
#include "lantern.h"
#include "firework.h"
#include "dragon.h"

#define WINDOW_WIDTH  1920
#define WINDOW_HEIGHT 1080

// --- Reparto proporcional del N total entre los 3 tipos ---
#define RATIO_DRAGONS   0.45f   // este porcentaje de N = total de SEGMENTOS de cuerpo de dragon
#define RATIO_FIREWORKS 0.40f
#define RATIO_LANTERNS  0.15f

// --- Constantes de diseno de cada tipo (no son "N", son detalles fijos) ---
#define MAX_BODY_PER_DRAGON     15     // tope de rombos por dragon; el resto crea otro dragon
#define HEAD_SCALE               0.20f
#define BODY_HALF_WIDTH          30.0f
#define BODY_HALF_HEIGHT         30.0f
#define SEGMENT_SPACING          20.0f
#define DRAGON_SPEED             110.0f

#define PARTICLES_PER_FIREWORK   40


typedef enum { ELEM_LANTERN, ELEM_FIREWORK, ELEM_DRAGON } ElemType;
typedef struct { ElemType type; int index; float depth; } RenderEntry;

static int compareByDepthDesc(const void* a, const void* b) {
    float da = ((const RenderEntry*)a)->depth;
    float db = ((const RenderEntry*)b)->depth;
    return (da < db) - (da > db); // descendente: lejos primero, cerca al final
}

int main(int argc, char* argv[]) {

    // --- Programacion defensiva: validar argumentos ---
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <N>\n", argv[0]);
        fprintf(stderr, "  N: cantidad total de elementos a renderizar\n");
        fprintf(stderr, "     (se reparte entre dragones, fuegos artificiales y lamparas)\n");
        return 1;
    }
    int N = atoi(argv[1]);
    if (N <= 0) {
        fprintf(stderr, "Error: N debe ser un entero positivo. Recibido: '%s'\n", argv[1]);
        return 1;
    }

    srand((unsigned int)time(NULL));

    // ================= REPARTO DE N =================
    int dragonBodyTotal = (int)(N * RATIO_DRAGONS);
    int numFireworks    = (int)(N * RATIO_FIREWORKS);
    int numLanterns     = N - dragonBodyTotal - numFireworks; // el resto exacto

    if (dragonBodyTotal == 0) dragonBodyTotal = 1;
    if (numFireworks == 0)    numFireworks = 1;
    if (numLanterns == 0)     numLanterns = 1;

    // Cuantos dragones hacen falta para repartir dragonBodyTotal segmentos,
    // con un maximo de MAX_BODY_PER_DRAGON cada uno (division hacia arriba).
    int numDragons = (dragonBodyTotal + MAX_BODY_PER_DRAGON - 1) / MAX_BODY_PER_DRAGON;

    printf("N total = %d\n", N);
    printf("  Dragones: %d dragon(es), %d segmentos de cuerpo en total (max %d por dragon)\n",
           numDragons, dragonBodyTotal, MAX_BODY_PER_DRAGON);
    printf("  Fuegos artificiales: %d\n", numFireworks);
    printf("  Lamparas: %d\n", numLanterns);

    // ================= RESERVA DE MEMORIA =================
    Dragon* dragons = malloc(numDragons * sizeof(Dragon));
    Firework* fireworks = malloc(numFireworks * sizeof(Firework));
    Particle* particles = malloc((size_t)numFireworks * PARTICLES_PER_FIREWORK * sizeof(Particle));
    Lantern* lanterns = malloc(numLanterns * sizeof(Lantern));

    if (!dragons || !fireworks || !particles || !lanterns) {
        fprintf(stderr, "Error: fallo al reservar memoria.\n");
        free(dragons); free(fireworks); free(particles); free(lanterns);
        return 1;
    }

    // ================= INICIALIZACION =================

    // Dragones: cada uno recibe hasta MAX_BODY_PER_DRAGON segmentos de cuerpo;
    // el ultimo dragon se queda con lo que sobre (puede ser menos de 15).
    int remainingBody = dragonBodyTotal;
    for (int i = 0; i < numDragons; i++) {
        int bodyCount = (remainingBody > MAX_BODY_PER_DRAGON) ? MAX_BODY_PER_DRAGON : remainingBody;
        remainingBody -= bodyCount;
        int numSegments = bodyCount + 1; // +1 por la cabeza

        float startX = 80 + (float)(rand() % (WINDOW_WIDTH - 160));
        float startY = 80 + (float)(rand() % (WINDOW_HEIGHT - 160));

        initDragon(&dragons[i], numSegments, SEGMENT_SPACING,
                   startX, startY, DRAGON_SPEED, WINDOW_WIDTH, WINDOW_HEIGHT);
    }

    // Fuegos artificiales
    initFireworks(fireworks, numFireworks, particles, PARTICLES_PER_FIREWORK,
                  WINDOW_WIDTH, WINDOW_HEIGHT);

    // Lamparas
    for (int i = 0; i < numLanterns; i++) {
        lanterns[i].baseX = 30 + (rand() % (WINDOW_WIDTH - 120));
        lanterns[i].baseY = 80 + (rand() % (WINDOW_HEIGHT - 300));
        lanterns[i].amplitudeX = 8 + (rand() % 20);
        lanterns[i].amplitudeY = 40 + (rand() % 12);
        lanterns[i].frequency = 0.5f + (rand() % 100) / 100.0f;
        lanterns[i].phase = (rand() % 628) / 100.0f;
        lanterns[i].driftY = 0.0f;
        lanterns[i].depth = 0.6f + (rand() % 100) / 100.0f;  // rango 0.6-1.6
        lanterns[i].x = lanterns[i].baseX;
        lanterns[i].y = lanterns[i].baseY;
    }

    // ================= VENTANA SDL =================
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Error al inicializar SDL: %s\n", SDL_GetError());
        free(dragons); free(fireworks); free(particles); free(lanterns);
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Festival Chino - Screensaver",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "Error al crear la ventana: %s\n", SDL_GetError());
        free(dragons); free(fireworks); free(particles); free(lanterns);
        SDL_Quit();
        return 1;
    }

    // SOFTWARE = todo el render corre por CPU (requisito del lab).
    // TARGETTEXTURE = necesario porque la cabeza del dragon se dibuja
    // primero a una textura interna y luego se rota/escala.
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE
    );
    if (!renderer) {
        fprintf(stderr, "Error al crear el renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        free(dragons); free(fireworks); free(particles); free(lanterns);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    buildLanternTemplate(); // una sola vez, antes del loop

    // ================= LOOP PRINCIPAL =================
    Uint32 startTicks = SDL_GetTicks();
    Uint32 lastTicks = startTicks;
    Uint32 frameCount = 0, fpsTimerStart = startTicks;
    int running = 1;
    SDL_Event event;
    char title[160];
    


   int totalElems = numDragons + numFireworks + numLanterns;
   RenderEntry* renderOrder = malloc(totalElems * sizeof(RenderEntry));
   int idx = 0;
   for (int i = 0; i < numDragons; i++)   renderOrder[idx++] = (RenderEntry){ ELEM_DRAGON,   i, dragons[i].depth };
   for (int i = 0; i < numFireworks; i++) renderOrder[idx++] = (RenderEntry){ ELEM_FIREWORK, i, fireworks[i].depth };
   for (int i = 0; i < numLanterns; i++)  renderOrder[idx++] = (RenderEntry){ ELEM_LANTERN,  i, lanterns[i].depth };
   qsort(renderOrder, totalElems, sizeof(RenderEntry), compareByDepthDesc);

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = 0;
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - lastTicks) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f; // evita saltos si un frame tarda mucho
        lastTicks = now;
        float elapsedTime = (now - startTicks) / 1000.0f;

        // --- UPDATE (secuencial por ahora; esto es lo que paralelizaremos con OpenMP) ---
        for (int i = 0; i < numDragons; i++)
            updateDragon(&dragons[i], dt, WINDOW_WIDTH, WINDOW_HEIGHT);

        for (int i = 0; i < numFireworks; i++)
            updateFirework(&fireworks[i], particles, dt, WINDOW_WIDTH, WINDOW_HEIGHT);

        for (int i = 0; i < numLanterns; i++)
            updateLantern(&lanterns[i], elapsedTime);

        // --- RENDER ---
        SDL_SetRenderDrawColor(renderer, 12, 12, 30, 255);
        SDL_RenderClear(renderer);

        for (int i = 0; i < totalElems; i++) {
            RenderEntry* e = &renderOrder[i];
            switch (e->type) {
                case ELEM_LANTERN:  renderLantern(renderer, &lanterns[e->index]); break;
                case ELEM_FIREWORK: renderFirework(renderer, &fireworks[e->index], particles); break;
                case ELEM_DRAGON:   renderDragon(renderer, &dragons[e->index], HEAD_SCALE, BODY_HALF_WIDTH, BODY_HALF_HEIGHT, 0); break;
            }
        }
        SDL_RenderPresent(renderer);

        // --- FPS ---
        frameCount++;
        Uint32 elapsedMs = now - fpsTimerStart;
        if (elapsedMs >= 1000) {
            double fps = frameCount / (elapsedMs / 1000.0);
            snprintf(title, sizeof(title),
                     "Festival Chino - N=%d (D:%d F:%d L:%d) - FPS: %.2f",
                     N, numDragons, numFireworks, numLanterns, fps);
            SDL_SetWindowTitle(window, title);
            frameCount = 0;
            fpsTimerStart = now;
        }
    }

    // ================= LIBERACION DE MEMORIA =================
    for (int i = 0; i < numDragons; i++) freeDragon(&dragons[i]);
    free(dragons);
    free(fireworks);
    free(particles);
    free(lanterns);
    free(renderOrder);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}