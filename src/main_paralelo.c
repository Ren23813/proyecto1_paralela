#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include "elements.h"
#include "lantern.h"
#include "firework.h"
#include "dragon.h"
#include "framebuffer.h"

#define WINDOW_WIDTH  1920
#define WINDOW_HEIGHT 1080

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

// --- Bandas de render por hilo ---
// Se reparte la pantalla en bandas horizontales de filas y cada hilo
// dibuja UNA banda completa (todos los elementos, recortados a esa banda)
// en el framebuffer. Como las bandas son disjuntas, dos hilos jamas
// escriben el mismo pixel -> no hace falta ningun lock.
//
// Usamos mas bandas que hilos para poder usar schedule(dynamic): los
// elementos no se reparten parejo en la pantalla (un dragon puede tener
// hasta 15 rombos concentrados en una zona chica), asi que con bandas mas
// finas, un hilo al que le toca una banda "vacia" termina rapido y agarra
// la siguiente banda libre, en vez de quedar ocioso esperando a un hilo
// al que le toco una banda cargada.
#define BANDS_PER_THREAD 4


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

    // ================= REPARTO DE N =================
    int dragonBodyTotal = (int)(N * RATIO_DRAGONS);
    int numFireworks    = (int)(N * RATIO_FIREWORKS);
    int numLanterns     = N - dragonBodyTotal - numFireworks;

    if (dragonBodyTotal == 0) dragonBodyTotal = 1;
    if (numFireworks == 0)    numFireworks = 1;
    if (numLanterns == 0)     numLanterns = 1;

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

    // ================= FRAMEBUFFER =================
    // Ver el comentario largo en main_secuencial.c: en vez de dibujar con
    // llamadas de SDL_Renderer (no paralelizables, con mucho overhead por
    // pixel), dibujamos sobre un arreglo de pixeles en RAM que se reparte
    // en bandas de filas entre los hilos, y se sube a pantalla con una
    // sola textura por frame.
    FrameBuffer* fb = fbCreate(WINDOW_WIDTH, WINDOW_HEIGHT);
    SDL_Texture* screenTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                                SDL_TEXTUREACCESS_STREAMING,
                                                WINDOW_WIDTH, WINDOW_HEIGHT);
    if (!fb || !screenTex) {
        fprintf(stderr, "Error al crear el framebuffer/textura de pantalla.\n");
        if (fb) fbDestroy(fb);
        if (screenTex) SDL_DestroyTexture(screenTex);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        free(dragons); free(fireworks); free(particles); free(lanterns);
        SDL_Quit();
        return 1;
    }

    buildLanternTemplate();       // una sola vez, antes del loop
    initDragonHeadArt(renderer);  // idem: hornea el arte de la cabeza a RAM

    int numBands = omp_get_max_threads() * BANDS_PER_THREAD;
    if (numBands > WINDOW_HEIGHT) numBands = WINDOW_HEIGHT;
    if (numBands < 1) numBands = 1;
    int bandHeight = (WINDOW_HEIGHT + numBands - 1) / numBands;

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
        // siguiente.
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

        // ================= RENDER PARALELO (la parte que antes era 100%
        // secuencial y dominaba el frame) =================
        //
        // Se reparte la pantalla en `numBands` bandas horizontales de
        // filas. Cada banda es procesada por un hilo (schedule dynamic
        // para balancear carga, ver comentario de BANDS_PER_THREAD). Dentro
        // de una banda, un hilo:
        //   1) limpia sus propias filas del framebuffer
        //   2) recorre TODOS los elementos en el mismo orden ya ordenado
        //      por profundidad (renderOrder) y dibuja solo la parte de
        //      cada uno que cae dentro de su banda -- cuerpo Y cabeza de
        //      cada dragon incluidos, ya que ninguno de los dos depende
        //      de SDL_Renderer.
        //
        // Como las bandas son disjuntas (cada fila pertenece a un solo
        // hilo), dos hilos JAMAS escriben el mismo pixel del framebuffer:
        // no hace falta ningun lock ni seccion critica. Y como cada hilo
        // procesa los elementos en el mismo orden global de profundidad,
        // el "pintor" (atras hacia adelante) se sigue respetando bien
        // dentro de cada banda -- el resultado visual es identico al de
        // la version secuencial (cabeza incluida, en su lugar correcto de
        // profundidad), solo que calculado en paralelo.
        #pragma omp parallel for schedule(dynamic)
        for (int band = 0; band < numBands; band++) {
            int y0 = band * bandHeight;
            int y1 = y0 + bandHeight;
            if (y1 > WINDOW_HEIGHT) y1 = WINDOW_HEIGHT;
            if (y0 >= y1) continue;

            fbClearRows(fb, 12, 12, 30, y0, y1);
            for (int i = 0; i < totalElems; i++) {
                RenderEntry* e = &renderOrder[i];
                switch (e->type) {
                    case ELEM_LANTERN:
                        renderLantern(fb, &lanterns[e->index], y0, y1);
                        break;
                    case ELEM_FIREWORK:
                        renderFirework(fb, &fireworks[e->index], particles, y0, y1);
                        break;
                    case ELEM_DRAGON:
                        renderDragon(fb, &dragons[e->index], HEAD_SCALE, BODY_HALF_WIDTH, BODY_HALF_HEIGHT,
                                     y0, y1);
                        break;
                }
            }
        } // <- barrera implicita: todas las bandas terminaron antes de subir la textura

        // Subir el framebuffer completo a la pantalla de una sola vez --
        // ya no hace falta ningun paso aparte despues de esto.
        SDL_UpdateTexture(screenTex, NULL, fb->pixels, WINDOW_WIDTH * (int)sizeof(Uint32));
        SDL_RenderCopy(renderer, screenTex, NULL, NULL);

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
    fbDestroy(fb);
    SDL_DestroyTexture(screenTex);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
