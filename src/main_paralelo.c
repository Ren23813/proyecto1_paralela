#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include "elements.h"
#include "lantern.h"
#include "firework.h"
#include "dragon.h"
#include <string.h>

#define WINDOW_WIDTH  1920
#define WINDOW_HEIGHT 1080

#define BENCHMARK_FRAMES 500
#define BENCHMARK_DT     0.016f   // dt fijo simulado (~60 fps), para que sea reproducible

// --- Reparto proporcional del N total entre los 3 tipos ---
#define RATIO_DRAGONS   0.45f
#define RATIO_FIREWORKS 0.40f
#define RATIO_LANTERNS  0.15f

// --- Constantes de diseno de cada tipo (no son "N", son detalles fijos) ---
#define MAX_BODY_PER_DRAGON     15
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
    return (da < db) - (da > db);
}

int main(int argc, char* argv[]) {

    // --- Programacion defensiva: validar argumentos ---
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <N> [num_hilos]\n", argv[0]);
        fprintf(stderr, "  N: cantidad total de elementos a renderizar\n");
        fprintf(stderr, "     (se reparte entre dragones, fuegos artificiales y lamparas)\n");
        fprintf(stderr, "  num_hilos: opcional. Si se omite, usa el maximo disponible.\n");
        return 1;
    }
    int N = atoi(argv[1]);
    if (N <= 0) {
        fprintf(stderr, "Error: N debe ser un entero positivo. Recibido: '%s'\n", argv[1]);
        return 1;
    }

    // --- Hilos de OpenMP: parametrizable, para poder medir speedup con distintos valores ---
    if (argc >= 3) {
        int numThreads = atoi(argv[2]);
        if (numThreads <= 0) {
            fprintf(stderr, "Error: num_hilos debe ser un entero positivo. Recibido: '%s'\n", argv[2]);
            return 1;
        }
        omp_set_num_threads(numThreads);
    }
    printf("OpenMP: usando hasta %d hilo(s)\n", omp_get_max_threads());

    srand((unsigned int)time(NULL));



    int benchmarkMode = (argc >= 4 && strcmp(argv[3], "--benchmark") == 0);  // paralelo


        
    // ================= REPARTO DE N =================
    int dragonBodyTotal = (int)(N * RATIO_DRAGONS);
    int numFireworks    = (int)(N * RATIO_FIREWORKS);
    int numLanterns     = N - dragonBodyTotal - numFireworks;

    if (dragonBodyTotal == 0) dragonBodyTotal = 1;
    if (numFireworks == 0)    numFireworks = 1;
    if (numLanterns == 0)     numLanterns = 1;

    int numDragons = (dragonBodyTotal + MAX_BODY_PER_DRAGON - 1) / MAX_BODY_PER_DRAGON;

    if (!benchmarkMode) {
        printf("N total = %d\n", N);
        printf("  Dragones: %d dragon(es), %d segmentos de cuerpo en total (max %d por dragon)\n",
               numDragons, dragonBodyTotal, MAX_BODY_PER_DRAGON);
        printf("  Fuegos artificiales: %d\n", numFireworks);
        printf("  Lamparas: %d\n", numLanterns);
    }

    if (!benchmarkMode) {
            printf("OpenMP: usando hasta %d hilo(s)\n", omp_get_max_threads());
        }

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

    // ================= INICIALIZACION (secuencial: se hace una sola vez) =================
    int remainingBody = dragonBodyTotal;
    for (int i = 0; i < numDragons; i++) {
        int bodyCount = (remainingBody > MAX_BODY_PER_DRAGON) ? MAX_BODY_PER_DRAGON : remainingBody;
        remainingBody -= bodyCount;
        int numSegments = bodyCount + 1;

        float startX = 80 + (float)(rand() % (WINDOW_WIDTH - 160));
        float startY = 80 + (float)(rand() % (WINDOW_HEIGHT - 160));

        initDragon(&dragons[i], numSegments, SEGMENT_SPACING,
                   startX, startY, DRAGON_SPEED, WINDOW_WIDTH, WINDOW_HEIGHT);
    }

    initFireworks(fireworks, numFireworks, particles, PARTICLES_PER_FIREWORK,
                  WINDOW_WIDTH, WINDOW_HEIGHT);

    for (int i = 0; i < numLanterns; i++) {
        lanterns[i].baseX = 30 + (rand() % (WINDOW_WIDTH - 120));
        lanterns[i].baseY = 80 + (rand() % (WINDOW_HEIGHT - 300));
        lanterns[i].amplitudeX = 8 + (rand() % 20);
        lanterns[i].amplitudeY = 40 + (rand() % 12);
        lanterns[i].frequency = 0.5f + (rand() % 100) / 100.0f;
        lanterns[i].phase = (rand() % 628) / 100.0f;
        lanterns[i].driftY = 0.0f;
        lanterns[i].depth = 0.6f + (rand() % 100) / 100.0f;
        lanterns[i].x = lanterns[i].baseX;
        lanterns[i].y = lanterns[i].baseY;
    }


    // ================= MODO BENCHMARK (sin ventana, mide solo el update) =================
    // int benchmarkMode = (argc >= 4 && strcmp(argv[3], "--benchmark") == 0);

    if (benchmarkMode) {
        double startTime = omp_get_wtime();

        for (int frame = 0; frame < BENCHMARK_FRAMES; frame++) {
            float elapsedTime = frame * BENCHMARK_DT;

            #pragma omp parallel
            {
                #pragma omp for schedule(dynamic) nowait
                for (int i = 0; i < numDragons; i++)
                    updateDragon(&dragons[i], BENCHMARK_DT, WINDOW_WIDTH, WINDOW_HEIGHT);

                #pragma omp for schedule(dynamic) nowait
                for (int i = 0; i < numFireworks; i++)
                    updateFirework(&fireworks[i], particles, BENCHMARK_DT, WINDOW_WIDTH, WINDOW_HEIGHT);

                #pragma omp for schedule(static)
                for (int i = 0; i < numLanterns; i++)
                    updateLantern(&lanterns[i], elapsedTime);
            }
        }

        double totalTime = omp_get_wtime() - startTime;

        // Salida en formato CSV: facil de parsear desde el script bash
        // version,N,hilos,frames,tiempo_total_seg,tiempo_promedio_por_frame_ms
        printf("paralelo,%d,%d,%d,%.6f,%.6f\n",
               N, omp_get_max_threads(), BENCHMARK_FRAMES,
               totalTime, (totalTime / BENCHMARK_FRAMES) * 1000.0);

        for (int i = 0; i < numDragons; i++) freeDragon(&dragons[i]);
        free(dragons); free(fireworks); free(particles); free(lanterns);
        return 0;
    }


    // ================= VENTANA SDL =================
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Error al inicializar SDL: %s\n", SDL_GetError());
        free(dragons); free(fireworks); free(particles); free(lanterns);
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Festival Chino - Screensaver (Paralelo)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "Error al crear la ventana: %s\n", SDL_GetError());
        free(dragons); free(fireworks); free(particles); free(lanterns);
        SDL_Quit();
        return 1;
    }

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

    buildLanternTemplate();

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
        if (dt > 0.05f) dt = 0.05f;
        lastTicks = now;
        float elapsedTime = (now - startTicks) / 1000.0f;

        // ================= UPDATE PARALELO =================
        // Descomposicion de dominio: cada tipo de elemento se reparte entre
        // hilos. Los 3 "for" comparten un solo equipo de hilos (una sola
        // region parallel) para no pagar el costo de crear hilos 3 veces
        // por frame. "nowait" en los dos primeros porque los arreglos
        // (dragons, fireworks/particles, lanterns) son independientes entre
        // si -- no hace falta esperar a que termine uno para empezar el
        // siguiente. schedule(dynamic) en dragones/fuegos porque la carga
        // de trabajo varia por elemento (un dragon con 15 segmentos cuesta
        // mas que uno con 2; un firework EXPLODED con particulas activas
        // cuesta mas que uno esperando su cooldown).
        #pragma omp parallel
        {
            #pragma omp for schedule(dynamic) nowait
            for (int i = 0; i < numDragons; i++)
                updateDragon(&dragons[i], dt, WINDOW_WIDTH, WINDOW_HEIGHT);

            #pragma omp for schedule(dynamic) nowait
            for (int i = 0; i < numFireworks; i++)
                updateFirework(&fireworks[i], particles, dt, WINDOW_WIDTH, WINDOW_HEIGHT);

            #pragma omp for schedule(static)
            for (int i = 0; i < numLanterns; i++)
                updateLantern(&lanterns[i], elapsedTime);
        } // <- barrera implicita acá: se asegura que todo terminó antes de dibujar

        // ================= RENDER (secuencial: SDL no es thread-safe) =================
        SDL_SetRenderDrawColor(renderer, 12, 12, 30, 255);
        SDL_RenderClear(renderer);

        for (int i = 0; i < totalElems; i++) {
            RenderEntry* e = &renderOrder[i];
            switch (e->type) {
                case ELEM_LANTERN:  renderLantern(renderer, &lanterns[e->index]); break;
                case ELEM_FIREWORK: renderFirework(renderer, &fireworks[e->index], particles); break;
                case ELEM_DRAGON:   renderDragon(renderer, &dragons[e->index], HEAD_SCALE, BODY_HALF_WIDTH, BODY_HALF_HEIGHT); break;
            }
        }
        SDL_RenderPresent(renderer);

        // --- FPS ---
        frameCount++;
        Uint32 elapsedMs = now - fpsTimerStart;
        if (elapsedMs >= 1000) {
            double fps = frameCount / (elapsedMs / 1000.0);
            snprintf(title, sizeof(title),
                     "Festival Chino [Paralelo, %d hilos] - N=%d (D:%d F:%d L:%d) - FPS: %.2f",
                     omp_get_max_threads(), N, numDragons, numFireworks, numLanterns, fps);
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
