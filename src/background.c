// src/background.c
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "background.h"

Uint32* loadBackgroundImage(const char* path, int expectedW, int expectedH) {
    SDL_Surface* raw = IMG_Load(path);
    if (!raw) {
        fprintf(stderr, "Aviso: no se pudo cargar el fondo '%s': %s (se sigue sin fondo)\n",
                path, IMG_GetError());
        return NULL;
    }

    if (raw->w != expectedW || raw->h != expectedH) {
        fprintf(stderr,
            "Aviso: '%s' mide %dx%d y se esperaba %dx%d -- se carga igual, "
            "recortando/rellenando con transparente lo que sobre o falte.\n",
            path, raw->w, raw->h, expectedW, expectedH);
    }

    // Forzamos el mismo formato de pixel que usa el framebuffer
    // (RGBA8888), asi el valor de cada pixel ya queda empaquetado
    // exactamente como fbPackRGBA8888 lo espera -- podemos copiarlo
    // directo, sin desempaquetar/reempaquetar pixel por pixel.
    SDL_Surface* conv = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(raw);
    if (!conv) {
        fprintf(stderr, "Aviso: no se pudo convertir el formato de '%s': %s (se sigue sin fondo)\n",
                path, SDL_GetError());
        return NULL;
    }

    Uint32* pixels = malloc(sizeof(Uint32) * (size_t)expectedW * (size_t)expectedH);
    if (!pixels) {
        fprintf(stderr, "Aviso: sin memoria para el fondo (se sigue sin fondo)\n");
        SDL_FreeSurface(conv);
        return NULL;
    }

    int copyW = (conv->w < expectedW) ? conv->w : expectedW;
    int copyH = (conv->h < expectedH) ? conv->h : expectedH;

    SDL_LockSurface(conv);
    for (int y = 0; y < expectedH; y++) {
        Uint32* dstRow = pixels + (size_t)y * expectedW;
        if (y < copyH) {
            const Uint32* srcRow = (const Uint32*)((const Uint8*)conv->pixels + (size_t)y * conv->pitch);
            for (int x = 0; x < expectedW; x++) {
                dstRow[x] = (x < copyW) ? srcRow[x] : 0u; // 0 = transparente
            }
        } else {
            for (int x = 0; x < expectedW; x++) dstRow[x] = 0u;
        }
    }
    SDL_UnlockSurface(conv);
    SDL_FreeSurface(conv);

    return pixels;
}

void freeBackgroundImage(Uint32* pixels) {
    free(pixels);
}