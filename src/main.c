#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "elements.h"
#include "lantern.h"

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600

int main(int argc, char* argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Uso: %s <N>\n", argv[0]);
        fprintf(stderr, "  N: cantidad de lamparas a renderizar\n");
        return 1;
    }
    int N = atoi(argv[1]);
    if (N <= 0) {
        fprintf(stderr, "Error: N debe ser un entero positivo. Recibido: '%s'\n", argv[1]);
        return 1;
    }

    Lantern* lanterns = malloc(N * sizeof(Lantern));
    if (lanterns == NULL) {
        fprintf(stderr, "Error: no se pudo reservar memoria para %d lamparas.\n", N);
        return 1;
    }

    srand((unsigned int)time(NULL));
    for (int i = 0; i < N; i++) {
        lanterns[i].baseX = 60 + (rand() % (WINDOW_WIDTH - 120));
        lanterns[i].baseY = 80 + (rand() % (WINDOW_HEIGHT - 300));
        lanterns[i].amplitudeX = 15 + (rand() % 20);
        lanterns[i].amplitudeY = 8 + (rand() % 12);
        lanterns[i].frequency = 0.5f + (rand() % 100) / 100.0f;
        lanterns[i].phase = (rand() % 628) / 100.0f;  // 0 a 2*PI aprox.
        lanterns[i].driftY = (rand() % 628) / 800.0f;   
        lanterns[i].x = lanterns[i].baseX;
        lanterns[i].y = lanterns[i].baseY;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Error al inicializar SDL: %s\n", SDL_GetError());
        free(lanterns);
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Festival Chino :D - Avance de Lamparas",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "Error al crear la ventana: %s\n", SDL_GetError());
        free(lanterns);
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        fprintf(stderr, "Error al crear el renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        free(lanterns);
        SDL_Quit();
        return 1;
    }

    buildLanternTemplate();   // se calcula una sola vez, antes del loop

    Uint32 startTicks = SDL_GetTicks();
    Uint32 frameCount = 0, fpsTimerStart = startTicks;
    int running = 1;
    SDL_Event event;
    char title[128];

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = 0;
        }

        float elapsedTime = (SDL_GetTicks() - startTicks) / 1000.0f;

        // por ahora secuencial, eventualmente será OpenMP 
        for (int i = 0; i < N; i++) {
            updateLantern(&lanterns[i], elapsedTime);
        }

        // render 
        SDL_SetRenderDrawColor(renderer, 10, 10, 40, 255);
        SDL_RenderClear(renderer);

        for (int i = 0; i < N; i++) {
            renderLantern(renderer, &lanterns[i]);
        }

        SDL_RenderPresent(renderer);

        frameCount++;
        Uint32 elapsedMs = SDL_GetTicks() - fpsTimerStart;
        if (elapsedMs >= 1000) {
            double fps = frameCount / (elapsedMs / 1000.0);
            snprintf(title, sizeof(title), "Festival Chino :D - N=%d - FPS: %.2f", N, fps);
            SDL_SetWindowTitle(window, title);
            frameCount = 0;
            fpsTimerStart = SDL_GetTicks();
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    free(lanterns);
    SDL_Quit();

    return 0;
}
