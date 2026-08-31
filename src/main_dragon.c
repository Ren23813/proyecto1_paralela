#include <SDL2/SDL.h>
#include <stdio.h>
#include "elements.h"
#include "dragon.h"

// Canvas del prototipo original de la cabeza (dragon_sdl.c usaba 900x650)
#define WINDOW_WIDTH  900
#define WINDOW_HEIGHT 650

// main_dragon: por ahora NO anima nada. Es una ventana "estatica" nada mas
// para poder ver la cabeza del dragon mientras se ajustan las piezas
// (posiciones, tamanos, colores) en dragon.c. Cuando se integre con la
// struct Dragon (segments) y su animacion, este main se parecera mas a
// main.c / main_fuegos.c (con update + loop de tiempo).
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Error al inicializar SDL: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Festival Chino - Dragon (vista estatica)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "Error al crear la ventana: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        fprintf(stderr, "Error al crear el renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    int running = 1;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = 0;
        }

        // Fondo nocturno, igual que en las demas escenas del festival
        SDL_SetRenderDrawColor(renderer, 10, 10, 40, 255);
        SDL_RenderClear(renderer);

        renderDragonHead(renderer);
        
        // --- Opción 1: Renderizar un solo segmento ---
        Segment miSegmento = { .x = 400.0, .y = 400.0, .angle = 45.0 };

        // Ancho de 40px (halfWidth = 20) y Alto de 30px (halfHeight = 15)
        renderDragonBodySegment(renderer, &miSegmento, 20.0, 15.0);


        SDL_RenderPresent(renderer);

        // Sin animacion todavia -> no hace falta redibujar a maxima
        // velocidad; esto ahorra CPU mientras solo estamos mirando la figura.
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}