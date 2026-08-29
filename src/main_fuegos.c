#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "elements.h"
#include "firework.h"

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600
#define PARTICLES_PER_FIREWORK 40   // constante de quemada

int main(int argc, char* argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Uso: %s <N>\n", argv[0]);
        fprintf(stderr, "  N: cantidad de fuegos artificiales a renderizar\n");
        return 1;
    }
    int N = atoi(argv[1]);
    if (N <= 0) {
        fprintf(stderr, "Error: N debe ser un entero positivo. Recibido: '%s'\n", argv[1]);
        return 1;
    }

    Firework* fireworks = malloc(N * sizeof(Firework));
    Particle* particles = malloc((size_t)N * PARTICLES_PER_FIREWORK * sizeof(Particle));
    if (fireworks == NULL || particles == NULL) {
        fprintf(stderr, "Error: no se pudo reservar memoria.\n");
        free(fireworks); free(particles);
        return 1;
    }

    srand((unsigned int)time(NULL));
    initFireworks(fireworks, N, particles, PARTICLES_PER_FIREWORK, WINDOW_WIDTH, WINDOW_HEIGHT);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Error al inicializar SDL: %s\n", SDL_GetError());
        free(fireworks); free(particles);
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Festival Chino - Prueba de Fuegos Artificiales",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN
    );
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

    Uint32 startTicks = SDL_GetTicks();
    Uint32 lastTicks = startTicks;
    Uint32 frameCount = 0, fpsTimerStart = startTicks;
    int running = 1;
    SDL_Event event;
    char title[128];

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = 0;
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - lastTicks) / 1000.0f;
        lastTicks = now;

        // --- Update (secuencial por ahora; paralelo después) ---
        for (int i = 0; i < N; i++) {
            updateFirework(&fireworks[i], particles, dt, WINDOW_WIDTH, WINDOW_HEIGHT);
        }

        // --- Render ---
        SDL_SetRenderDrawColor(renderer, 10, 10, 40, 255);
        SDL_RenderClear(renderer);

        for (int i = 0; i < N; i++) {
            renderFirework(renderer, &fireworks[i], particles);
        }

        SDL_RenderPresent(renderer);

        frameCount++;
        Uint32 elapsedMs = now - fpsTimerStart;
        if (elapsedMs >= 1000) {
            double fps = frameCount / (elapsedMs / 1000.0);
            snprintf(title, sizeof(title), "Festival Chino - N=%d - FPS: %.2f", N, fps);
            SDL_SetWindowTitle(window, title);
            frameCount = 0;
            fpsTimerStart = now;
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    free(fireworks);
    free(particles);
    SDL_Quit();

    return 0;
}

