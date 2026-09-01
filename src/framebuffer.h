// src/framebuffer.h
//
// Por que existe este archivo:
// SDL_Renderer NO es thread-safe (no se puede llamar SDL_RenderDrawPoint /
// SDL_RenderCopy / etc. desde varios hilos a la vez sobre el mismo
// renderer). Por eso, en el diseño original, el render entero es
// secuencial en ambas versiones del programa -- el OpenMP solo tocaba el
// update. Como el render (en especial los rombos del cuerpo del dragon,
// pintados con SDL_RenderDrawPoint pixel por pixel) es la parte mas cara
// del frame, paralelizar solo el update no alcanza a mover la aguja del
// FPS.
//
// La solucion: dibujar "a mano" sobre un arreglo plano de pixeles en RAM
// (esto SI se puede hacer en paralelo, porque son simples escrituras a
// memoria, no llamadas a SDL), y subir ese arreglo a la pantalla con UNA
// sola textura al final del frame (SDL_UpdateTexture + SDL_RenderCopy).
//
// Para paralelizar el llenado del framebuffer sin necesitar locks: se
// reparte la pantalla en bandas horizontales de filas [yStart, yEnd)
// disjuntas entre si. Cada hilo dibuja TODOS los elementos pero recortados
// a su propia banda (nunca escribe filas de otra banda), asi que dos
// hilos jamas tocan el mismo pixel. Ademas, como cada hilo recorre los
// elementos en el mismo orden ya ordenado por profundidad (renderOrder),
// el algoritmo del pintor (atras -> adelante) se sigue respetando
// correctamente dentro de cada banda.
#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <SDL2/SDL.h>

typedef struct {
    Uint32* pixels;   // formato RGBA8888, fila por fila (row-major)
    int width;
    int height;
} FrameBuffer;

FrameBuffer* fbCreate(int width, int height);
void fbDestroy(FrameBuffer* fb);

// Limpia las filas [yStart, yEnd) al color dado (alpha 255). Se puede
// llamar por banda para paralelizar tambien el clear.
void fbClearRows(FrameBuffer* fb, Uint8 r, Uint8 g, Uint8 b, int yStart, int yEnd);

// --- Empaquetado/desempaquetado manual para RGBA8888 ---
// Evita llamar a SDL_MapRGBA/SDL_GetRGBA (funciones de la libreria, con
// overhead de llamada a traves del .so) en el hot path de pixel a pixel.
// Si el formato de la textura cambia, hay que actualizar esto tambien.
static inline Uint32 fbPackRGBA8888(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    return ((Uint32)r << 24) | ((Uint32)g << 16) | ((Uint32)b << 8) | (Uint32)a;
}
static inline void fbUnpackRGBA8888(Uint32 v, Uint8* r, Uint8* g, Uint8* b, Uint8* a) {
    *r = (Uint8)(v >> 24);
    *g = (Uint8)(v >> 16);
    *b = (Uint8)(v >> 8);
    *a = (Uint8)(v);
}

// Escritura directa (sobreescribe, no mezcla) para formas opacas -- es lo
// que reemplaza a los SDL_SetRenderDrawColor+SDL_RenderDrawPoint por
// pixel que tenia fill_diamond_gradient original.
static inline void fbSetPixel(FrameBuffer* fb, int x, int y, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    if ((unsigned)x >= (unsigned)fb->width || (unsigned)y >= (unsigned)fb->height) return;
    fb->pixels[(size_t)y * fb->width + x] = fbPackRGBA8888(r, g, b, a);
}

// Mezcla alpha "source over" contra lo que ya hay en el buffer (para
// particulas que se desvanecen, etc. -- equivalente a SDL_BLENDMODE_BLEND).
void fbBlendPixel(FrameBuffer* fb, int x, int y, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

// --- Primitivas de dibujo. TODAS reciben [yStart, yEnd) y solo tocan
// pixeles dentro de ese rango de filas -- esto es lo que permite repartir
// la pantalla en bandas entre hilos sin ningun lock. ---

void fbFillCircle(FrameBuffer* fb, int cx, int cy, int radius,
                   Uint8 r, Uint8 g, Uint8 b, Uint8 a, int yStart, int yEnd);

void fbThickLine(FrameBuffer* fb, double x0, double y0, double x1, double y1,
                  int thickness, Uint8 r, Uint8 g, Uint8 b, Uint8 a, int yStart, int yEnd);

void fbCircleOutline(FrameBuffer* fb, int cx, int cy, int radius,
                      Uint8 r, Uint8 g, Uint8 b, Uint8 a, int yStart, int yEnd);

// Poligono generico relleno de un solo color (scanline fill), usado por el
// cuerpo/tapas del farol.
void fbFillPolygon(FrameBuffer* fb, const SDL_Point* pts, int n,
                    Uint8 r, Uint8 g, Uint8 b, Uint8 a, int yStart, int yEnd);

// Rombo con degradado horizontal (en espacio local, antes de rotar) de
// (lr,lg,lb) [borde izquierdo] a (rr,rg,rb) [borde derecho]. Usado por los
// segmentos de cuerpo de los dragones -- es la primitiva mas usada por
// lejos (hasta 45% de N), y la que antes hacia un SDL_RenderDrawPoint por
// pixel.
void fbFillDiamondGradient(FrameBuffer* fb, int cx, int cy,
                            double halfWidth, double halfHeight, double angle_deg,
                            Uint8 lr, Uint8 lg, Uint8 lb,
                            Uint8 rr, Uint8 rg, Uint8 rb,
                            int yStart, int yEnd);

#endif