// main_paralelo.c
//
// Version paralela del screensaver. Dos cosas se paralelizan con OpenMP:
//
//  1) UPDATE: dragones, fuegos artificiales y lamparas se actualizan en
//     paralelo (son independientes entre si dentro de cada tipo).
//
//  2) RENDER: en vez de un solo SDL_Renderer (que NO es thread-safe y no
//     se puede compartir entre hilos), se crean K "capas" de profundidad,
//     cada una con su PROPIA superficie (SDL_Surface) y su PROPIO renderer
//     independiente (SDL_CreateSoftwareRenderer). Cada hilo dibuja SOLO
//     su capa, sobre su propia memoria -> cero condiciones de carrera,
//     sin locks. Al final, un paso SECUENCIAL corto compone (blit) las K
//     superficies en orden de profundidad (de lejos a cerca, pintor's
//     algorithm) y sube el resultado a la ventana en una sola operacion.
//
// Compilar (ejemplo, ajusta include/lib paths de SDL2 si hace falta):
//   gcc main_paralelo.c dragon.c firework.c lantern.c -o screensaver_par \
//       -fopenmp -lSDL2 -lSDL2_gfx -lm -O2
//
// Correr:
//   ./screensaver_par <N>
//
// Variable de entorno util para experimentar:
//   OMP_NUM_THREADS=4 ./screensaver_par 20000

#include <SDL2/SDL.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <float.h>
#include "elements.h"
#include "lantern.h"
#include "firework.h"
#include "dragon.h"

#define WINDOW_WIDTH  1920
#define WINDOW_HEIGHT 1080

// --- Reparto proporcional del N total entre los 3 tipos ---
#define RATIO_DRAGONS   0.45f
#define RATIO_FIREWORKS 0.40f
#define RATIO_LANTERNS  0.15f

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
    return (da < db) - (da > db); // descendente: lejos (depth alto) primero
}

// A que capa pertenece una entidad segun su depth. Capa 0 = mas cerca
// (depth minimo), capa (numLayers-1) = mas lejos (depth maximo).
static int depthToLayer(float depth, float minDepth, float maxDepth, int numLayers) {
    if (maxDepth <= minDepth) return 0;
    float t = (depth - minDepth) / (maxDepth - minDepth);
    int layer = (int)(t * numLayers);
    if (layer >= numLayers) layer = numLayers - 1;
    if (layer < 0) layer = 0;
    return layer;
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Uso: %s <N>\n", argv[0]);
        return 1;
    }
    int N = atoi(argv[1]);
    if (N <= 0) {
        fprintf(stderr, "Error: N debe ser un entero positivo. Recibido: '%s'\n", argv[1]);
        return 1;
    }

    srand((unsigned int)time(NULL));

    // ================= REPARTO DE N (igual que la version secuencial) =================
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
    printf("  Hilos OpenMP disponibles: %d\n", omp_get_max_threads());

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

    // ================= INICIALIZACION (secuencial: es una sola vez) =================
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

    // ================= VENTANA SDL (renderer principal solo compone el frame final) =================
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Error al inicializar SDL: %s\n", SDL_GetError());
        free(dragons); free(fireworks); free(particles); free(lanterns);
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Festival Chino - Screensaver (paralelo)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "Error al crear la ventana: %s\n", SDL_GetError());
        free(dragons); free(fireworks); free(particles); free(lanterns);
        SDL_Quit();
        return 1;
    }

    // Sigue siendo software (requisito del lab: todo el render corre por CPU).
    // Este renderer principal YA NO dibuja figuras una por una: solo sube
    // la superficie final ya compuesta como una textura, una vez por frame.
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

    // ================= ORDEN DE PROFUNDIDAD (fijo: depth nunca cambia en runtime) =================
    int totalElems = numDragons + numFireworks + numLanterns;
    RenderEntry* renderOrder = malloc(totalElems * sizeof(RenderEntry));
    int idx = 0;
    for (int i = 0; i < numDragons; i++)   renderOrder[idx++] = (RenderEntry){ ELEM_DRAGON,   i, dragons[i].depth };
    for (int i = 0; i < numFireworks; i++) renderOrder[idx++] = (RenderEntry){ ELEM_FIREWORK, i, fireworks[i].depth };
    for (int i = 0; i < numLanterns; i++)  renderOrder[idx++] = (RenderEntry){ ELEM_LANTERN,  i, lanterns[i].depth };
    qsort(renderOrder, totalElems, sizeof(RenderEntry), compareByDepthDesc);

    // ================= SETUP DE CAPAS DE RENDER =================
    // Numero de capas = numero de hilos disponibles (con techo por el
    // limite de dragon.h y piso de al menos 1), y nunca mas que elementos.
    int numLayers = omp_get_max_threads();
    if (numLayers > DRAGON_MAX_RENDER_LAYERS) numLayers = DRAGON_MAX_RENDER_LAYERS;
    if (numLayers > totalElems) numLayers = totalElems;
    if (numLayers < 1) numLayers = 1;
    printf("  Capas de render: %d\n", numLayers);

    float minDepth = FLT_MAX, maxDepth = -FLT_MAX;
    for (int i = 0; i < totalElems; i++) {
        if (renderOrder[i].depth < minDepth) minDepth = renderOrder[i].depth;
        if (renderOrder[i].depth > maxDepth) maxDepth = renderOrder[i].depth;
    }

    // Bucketizar las entidades por capa UNA sola vez (el depth es fijo).
    // Como renderOrder ya viene ordenado descendente por depth, cada
    // bucket queda internamente ordenado tambien -> no hace falta volver
    // a ordenar dentro de cada capa.
    int* layerCount = calloc(numLayers, sizeof(int));
    for (int i = 0; i < totalElems; i++) {
        int l = depthToLayer(renderOrder[i].depth, minDepth, maxDepth, numLayers);
        layerCount[l]++;
    }
    RenderEntry** layerEntries = malloc(numLayers * sizeof(RenderEntry*));
    int* layerFillPos = calloc(numLayers, sizeof(int));
    for (int l = 0; l < numLayers; l++)
        layerEntries[l] = malloc((layerCount[l] > 0 ? layerCount[l] : 1) * sizeof(RenderEntry));
    for (int i = 0; i < totalElems; i++) {
        int l = depthToLayer(renderOrder[i].depth, minDepth, maxDepth, numLayers);
        layerEntries[l][layerFillPos[l]++] = renderOrder[i];
    }
    free(layerFillPos);

    // Una superficie + un renderer de software INDEPENDIENTE por capa.
    // Formato con canal alpha para poder componerlas con transparencia.
    SDL_Surface** layerSurfaces = malloc(numLayers * sizeof(SDL_Surface*));
    SDL_Renderer** layerRenderers = malloc(numLayers * sizeof(SDL_Renderer*));
    for (int l = 0; l < numLayers; l++) {
        layerSurfaces[l] = SDL_CreateRGBSurfaceWithFormat(
            0, WINDOW_WIDTH, WINDOW_HEIGHT, 32, SDL_PIXELFORMAT_RGBA8888);
        layerRenderers[l] = SDL_CreateSoftwareRenderer(layerSurfaces[l]);
        SDL_SetSurfaceBlendMode(layerSurfaces[l], SDL_BLENDMODE_BLEND);
    }

    // Superficie de composicion final + textura que se sube a la ventana.
    SDL_Surface* composeSurface = SDL_CreateRGBSurfaceWithFormat(
        0, WINDOW_WIDTH, WINDOW_HEIGHT, 32, SDL_PIXELFORMAT_RGBA8888);
    SDL_Texture* screenTexture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
        WINDOW_WIDTH, WINDOW_HEIGHT);

    // ================= LOOP PRINCIPAL =================
    Uint32 startTicks = SDL_GetTicks();
    Uint32 lastTicks = startTicks;
    Uint32 frameCount = 0, fpsTimerStart = startTicks;
    int running = 1;
    SDL_Event event;
    char title[160];

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

        // --- UPDATE EN PARALELO ---
        // Un solo "parallel" para las tres fases: evita crear/destruir
        // tres equipos de hilos por frame (uno por cada omp parallel for
        // separado). nowait en las dos primeras porque dragones, fuegos y
        // lamparas son independientes entre si (no hace falta esperar a
        // que termine un tipo para empezar otro); la ultima fase si deja
        // el barrier implicito, para garantizar que TODO el update
        // termino antes de pasar a leer esos datos en el render.
        //
        // NOTA: updateFirework() llama internamente a rand() (via
        // randRange en firework.c) para resetear/explotar fuegos.
        // rand() de glibc es thread-safe pero usa un lock interno, asi
        // que bajo carga puede serializar parcialmente este loop. Si se
        // quiere paralelismo real ahi, hay que migrar firework.c a
        // rand_r() con una semilla por hilo.
        #pragma omp parallel
        {
            #pragma omp for schedule(static) nowait
            for (int i = 0; i < numDragons; i++)
                updateDragon(&dragons[i], dt, WINDOW_WIDTH, WINDOW_HEIGHT);

            #pragma omp for schedule(dynamic, 4) nowait
            for (int i = 0; i < numFireworks; i++)
                updateFirework(&fireworks[i], particles, dt, WINDOW_WIDTH, WINDOW_HEIGHT);

            #pragma omp for schedule(static)
            for (int i = 0; i < numLanterns; i++)
                updateLantern(&lanterns[i], elapsedTime);
        } // barrier implicito al salir: aqui SI queremos esperar a todos

        // --- RENDER EN PARALELO POR CAPAS ---
        // Cada hilo limpia y dibuja SOLO su capa (su propia superficie +
        // su propio renderer). No hay memoria compartida en escritura
        // entre hilos aqui: cada layerSurfaces[l]/layerRenderers[l] es
        // exclusivo del hilo que procesa la capa l en esta iteracion.
        #pragma omp parallel for schedule(dynamic)
        for (int l = 0; l < numLayers; l++) {
            SDL_Renderer* lr = layerRenderers[l];

            // Limpiar a transparente total (blend NONE para que el clear
            // no se mezcle con el frame anterior).
            SDL_SetRenderDrawBlendMode(lr, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(lr, 0, 0, 0, 0);
            SDL_RenderClear(lr);
            SDL_SetRenderDrawBlendMode(lr, SDL_BLENDMODE_BLEND);

            for (int k = 0; k < layerCount[l]; k++) {
                RenderEntry* e = &layerEntries[l][k];
                switch (e->type) {
                    case ELEM_LANTERN:
                        renderLantern(lr, &lanterns[e->index]);
                        break;
                    case ELEM_FIREWORK:
                        renderFirework(lr, &fireworks[e->index], particles);
                        break;
                    case ELEM_DRAGON:
                        renderDragon(lr, &dragons[e->index], HEAD_SCALE,
                                     BODY_HALF_WIDTH, BODY_HALF_HEIGHT, l);
                        break;
                }
            }
        }

        // --- MERGE SECUENCIAL (rapido: solo blits de superficies ya listas) ---
        SDL_FillRect(composeSurface, NULL,
                     SDL_MapRGBA(composeSurface->format, 12, 12, 30, 255));
        for (int l = numLayers - 1; l >= 0; l--) {  // lejos (depth alto) -> cerca
            SDL_BlitSurface(layerSurfaces[l], NULL, composeSurface, NULL);
        }

        SDL_UpdateTexture(screenTexture, NULL, composeSurface->pixels, composeSurface->pitch);
        SDL_RenderCopy(renderer, screenTexture, NULL, NULL);
        SDL_RenderPresent(renderer);

        // --- FPS ---
        frameCount++;
        Uint32 elapsedMs = now - fpsTimerStart;
        if (elapsedMs >= 1000) {
            double fps = frameCount / (elapsedMs / 1000.0);
            snprintf(title, sizeof(title),
                     "Festival Chino [PARALELO] - N=%d (D:%d F:%d L:%d) capas:%d - FPS: %.2f",
                     N, numDragons, numFireworks, numLanterns, numLayers, fps);
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
    free(layerCount);
    for (int l = 0; l < numLayers; l++) free(layerEntries[l]);
    free(layerEntries);

    for (int l = 0; l < numLayers; l++) {
        SDL_DestroyRenderer(layerRenderers[l]);
        SDL_FreeSurface(layerSurfaces[l]);
    }
    free(layerRenderers);
    free(layerSurfaces);
    SDL_FreeSurface(composeSurface);
    SDL_DestroyTexture(screenTexture);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}