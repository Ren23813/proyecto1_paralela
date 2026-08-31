#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "elements.h"
#include "dragon.h"

#define WINDOW_WIDTH  900
#define WINDOW_HEIGHT 650

// Tamano de la cabeza vs el cuerpo: el dibujo original de la cabeza es
// grande (900x650), asi que HEAD_SCALE lo reduce a algo proporcional al
// ancho del rombo del cuerpo (BODY_HALF_WIDTH*2). Ajusta estos numeros
// juntos si cambias el tamano de cualquiera de las dos piezas.
#define HEAD_SCALE       0.20f
#define BODY_HALF_WIDTH  30.0f
#define BODY_HALF_HEIGHT 30.0f
#define SEGMENT_SPACING  20.0f  // menor que BODY_HALF_WIDTH*2 -> se solapan un poco, como en la imagen
#define DRAGON_SPEED     110.0f // pixeles por segundo

int main(int argc, char* argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Uso: %s <N>\n", argv[0]);
        fprintf(stderr, "  N: cantidad de rombos que forman el cuerpo (sin contar la cabeza)\n");
        return 1;
    }
    int bodyCount = atoi(argv[1]);
    if (bodyCount <= 0) {
        fprintf(stderr, "Error: N debe ser un entero positivo. Recibido: '%s'\n", argv[1]);
        return 1;
    }
    int numSegments = bodyCount + 1; // +1 por la cabeza (segments[0])

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Error al inicializar SDL: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Festival Chino - Dragon",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "Error al crear la ventana: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // SDL_RENDERER_TARGETTEXTURE: la cabeza se dibuja a una textura antes
    // de rotarla/escalarla, asi que el renderer tiene que soportar
    // render-to-texture.
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE
    );
    if (!renderer) {
        fprintf(stderr, "Error al crear el renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    srand((unsigned int)time(NULL));

    Dragon dragon;
    initDragon(&dragon, numSegments, SEGMENT_SPACING,
               WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f, DRAGON_SPEED,
               WINDOW_WIDTH, WINDOW_HEIGHT);

    Uint32 lastTicks = SDL_GetTicks();
    Uint32 frameCount = 0, fpsTimerStart = lastTicks;
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
        if (dt > 0.05f) dt = 0.05f; // evita saltos grandes si hubo un frame lento

        updateDragon(&dragon, dt, WINDOW_WIDTH, WINDOW_HEIGHT);

        // Fondo nocturno, igual que en las demas escenas del festival
        SDL_SetRenderDrawColor(renderer, 10, 10, 40, 255);
        SDL_RenderClear(renderer);

        renderDragon(renderer, &dragon, HEAD_SCALE, BODY_HALF_WIDTH, BODY_HALF_HEIGHT);

        SDL_RenderPresent(renderer);

        frameCount++;
        Uint32 elapsedMs = now - fpsTimerStart;
        if (elapsedMs >= 1000) {
            double fps = frameCount / (elapsedMs / 1000.0);
            snprintf(title, sizeof(title), "Festival Chino - Dragon - N=%d - FPS: %.2f", bodyCount, fps);
            SDL_SetWindowTitle(window, title);
            frameCount = 0;
            fpsTimerStart = now;
        }

        SDL_Delay(16);
    }

    freeDragon(&dragon);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}