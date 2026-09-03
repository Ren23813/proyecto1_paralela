#ifndef BACKGROUND_H
#define BACKGROUND_H

#include <SDL2/SDL.h>

//Carga una imagen de fondo (PNG/JPG) desde disco y la convierte a un búfer
//Si la imagen es más pequeña que la dimensión esperada, rellena los bordes con transparencia
//i la imagen es más grande, la recorta para ajustarla exactamente al búfer.
Uint32* loadBackgroundImage(const char* path, int expectedW, int expectedH);

// Libera lo devuelto por loadBackgroundImage. Seguro de llamar con NULL.
void freeBackgroundImage(Uint32* pixels);

#endif