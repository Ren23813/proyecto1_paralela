// src/background.h
//
// Carga la imagen de fondo (PNG con transparencia) a un buffer de pixeles
// en RAM, UNA sola vez al arranque, para poder componerla contra el
// framebuffer cada frame con fbCompositeFullscreen (en framebuffer.c) --
// asi las partes transparentes del PNG siguen dejando ver el color de
// fondo que se ponga en el codigo (fbClearRows), sin importar cual sea.
#ifndef BACKGROUND_H
#define BACKGROUND_H

#include <SDL2/SDL.h>

// Carga 'path' (PNG, o cualquier formato que soporte SDL_image) a un
// buffer RGBA8888 de EXACTAMENTE expectedW x expectedH pixeles -- si el
// archivo mide menos, el resto queda transparente; si mide mas, se
// recorta. Pensada para un fondo pre-exportado a la misma resolucion que
// la ventana (WINDOW_WIDTH x WINDOW_HEIGHT), asi que no hace falta
// escalar nada en tiempo de render.
//
// Devuelve NULL si no se pudo cargar (imprime el motivo en stderr) -- en
// ese caso el programa puede seguir andando sin fondo, no es un error
// fatal.
Uint32* loadBackgroundImage(const char* path, int expectedW, int expectedH);

// Libera lo devuelto por loadBackgroundImage. Seguro de llamar con NULL.
void freeBackgroundImage(Uint32* pixels);

#endif