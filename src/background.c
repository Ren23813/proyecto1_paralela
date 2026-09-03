#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "background.h"

//Implementación de la carga y acondicionamiento del fondo en RAM.
Uint32* loadBackgroundImage(const char* path, int expectedW, int expectedH) {

    //CARGA INICIAL DE LA IMAGEN
    SDL_Surface* raw = IMG_Load(path);
    if (!raw) {
        fprintf(stderr, "Aviso: no se pudo cargar el fondo '%s': %s (se sigue sin fondo)\n",
                path, IMG_GetError());
        return NULL;
    }

    // Validación y advertencia de discrepancia de dimensiones
    if (raw->w != expectedW || raw->h != expectedH) {
        fprintf(stderr,
            "Aviso: '%s' mide %dx%d y se esperaba %dx%d -- se carga igual, "
            "recortando/rellenando con transparente lo que sobre o falte.\n",
            path, raw->w, raw->h, expectedW, expectedH);
    }

    //  CONVERSIÓN DE FORMATO A RGBA8888
    // Normalizamos los píxeles al formato exacto del framebuffer (RGBA8888).
    // Esto optimiza el renderizado posterior al permitir copias directas de memoria
    // sin tener que descompilar canal por canal (R, G, B, A) en cada frame.
    SDL_Surface* conv = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA8888, 0);
    SDL_FreeSurface(raw);
    if (!conv) {
        fprintf(stderr, "Aviso: no se pudo convertir el formato de '%s': %s (se sigue sin fondo)\n",
                path, SDL_GetError());
        return NULL;
    }

    // ASIGNACIÓN DE MEMORIA PARA EL BÚFER FINAL
    Uint32* pixels = malloc(sizeof(Uint32) * (size_t)expectedW * (size_t)expectedH);
    if (!pixels) {
        fprintf(stderr, "Aviso: sin memoria para el fondo (se sigue sin fondo)\n");
        SDL_FreeSurface(conv);
        return NULL;
    }

    //// Determinamos la región válida de intersección para el copiado (clipping)
    int copyW = (conv->w < expectedW) ? conv->w : expectedW;
    int copyH = (conv->h < expectedH) ? conv->h : expectedH;

    //OPIADO Y ACONDICIONAMIENTO FILA POR FILA
    SDL_LockSurface(conv);
    for (int y = 0; y < expectedH; y++) {
        Uint32* dstRow = pixels + (size_t)y * expectedW;
        if (y < copyH) {
            //// Calculamos el inicio de la fila origen respetando el 'pitch' (bytes por fila)
            const Uint32* srcRow = (const Uint32*)((const Uint8*)conv->pixels + (size_t)y * conv->pitch);

            for (int x = 0; x < expectedW; x++) {
                // Copia el píxel si está dentro de los límites de la imagen, o llena con transparente
                dstRow[x] = (x < copyW) ? srcRow[x] : 0u; // 0 = transparente
            }
        } else {
            //// Filas sobrantes fuera de la altura de la imagen se rellenan completamente con transparencia
            for (int x = 0; x < expectedW; x++) dstRow[x] = 0u;
        }
    }
    SDL_UnlockSurface(conv);
    SDL_FreeSurface(conv); //LIMPIEZA DE RECURSOS TEMPORALES
    // Retorna el búfer listo para usarse en fbCompositeFullscreen
    return pixels;
}

//Libera el bloque de memoria asignado al fondo.
void freeBackgroundImage(Uint32* pixels) {
    free(pixels);
}