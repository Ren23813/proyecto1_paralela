#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H
// ARQUITECTURA DEL FRAMEBUFFER EN RAM (RENDERIZADO PARALELO)

// Por qué existe este archivo:
// SDL_Renderer NO es thread-safe (no se puede llamar a SDL_RenderDrawPoint,
// SDL_RenderCopy, etc. simultáneamente desde varios hilos sobre el mismo renderer).

// La solución: Dibujar "a mano" sobre un búfer plano de píxeles en RAM
// (escritura directa en memoria, 100% thread-safe si las regiones no se solapan),
// y transferir esa imagen final a la GPU mediante una sola textura al final
// del frame (SDL_UpdateTexture + SDL_RenderCopy).

// Para paralelizar sin cerrojos (locks): Se divide la pantalla verticalmente
// en bandas de filas horizontales disjuntas [yStart, yEnd). Cada hilo procesa
// TODOS los objetos pero recortados estrictamente a su propia banda, garantizando
// cero condiciones de carrera y preservando el orden de profundidad (Painter's Algorithm).

#include <SDL2/SDL.h>

//Estructura del Framebuffer alojado en RAM.
//Mantiene un arreglo unidimensional de píxeles dispuesto en orden row-major.
typedef struct {
    Uint32* pixels;   // formato RGBA8888,
    int width;      // Ancho en píxeles
    int height;
} FrameBuffer;

//Gestión de ciclo de vida del Framebuffer
FrameBuffer* fbCreate(int width, int height);
void fbDestroy(FrameBuffer* fb);

// Permite paralelizar el borrado de pantalla entre múltiples hilos por banda.
void fbClearRows(FrameBuffer* fb, Uint8 r, Uint8 g, Uint8 b, int yStart, int yEnd);

//// FUNCIONES INLINE PARA ACCESO RÁPIDO A PÍXELES (HOT PATH)
//Empaqueta componentes de color de 8 bits en un entero de 32 bits en formato RGBA8888.
//Evita el overhead de invocar funciones de biblioteca como SDL_MapRGBA en bucles intensivos.
static inline Uint32 fbPackRGBA8888(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    return ((Uint32)r << 24) | ((Uint32)g << 16) | ((Uint32)b << 8) | (Uint32)a;
}

//Desempaqueta un entero de 32 bits (RGBA8888) en sus componentes individuales de 8 bits.
static inline void fbUnpackRGBA8888(Uint32 v, Uint8* r, Uint8* g, Uint8* b, Uint8* a) {
    *r = (Uint8)(v >> 24);
    *g = (Uint8)(v >> 16);
    *b = (Uint8)(v >> 8);
    *a = (Uint8)(v);
}

//Escribe directamente un píxel opaco en las coordenadas (x, y) sobreescribiendo el fondo.
//Realiza verificación de límites
static inline void fbSetPixel(FrameBuffer* fb, int x, int y, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    if ((unsigned)x >= (unsigned)fb->width || (unsigned)y >= (unsigned)fb->height) return;
    fb->pixels[(size_t)y * fb->width + x] = fbPackRGBA8888(r, g, b, a);
}

//Mezcla un píxel semi-transparente sobre el color existente usando la fórmula
//de transparencia "Source Over" (equivalente a SDL_BLENDMODE_BLEND).
void fbBlendPixel(FrameBuffer* fb, int x, int y, Uint8 r, Uint8 g, Uint8 b, Uint8 a);



// PRIMITIVAS DE DIBUJO GEOMÉTRICO (COMPATIBLES CON PARALELISMO POR BANDAS)-

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

// Dibuja un rombo rotado con un degradado lineal horizontal en su espacio local.
// Diseñado específicamente para optimizar la representación de los segmentos del dragón.
void fbFillDiamondGradient(FrameBuffer* fb, int cx, int cy,
                            double halfWidth, double halfHeight, double angle_deg,
                            Uint8 lr, Uint8 lg, Uint8 lb,
                            Uint8 rr, Uint8 rg, Uint8 rb,
                            int yStart, int yEnd);

//Mapea una textura cargada en RAM recortada, rotada, escalada y tintada mediante
//muestreo de vecino más cercano e interpolación inversa de coordenadas.
void fbBlitRotatedTinted(FrameBuffer* fb, const Uint32* src, int srcW, int srcH,
                          double anchorSrcX, double anchorSrcY,
                          double dstX, double dstY, double scale, double angleDeg,
                          Uint8 tintR, Uint8 tintG, Uint8 tintB,
                          int yStart, int yEnd);

// Realiza un blit directo de alta velocidad para imágenes estáticas a pantalla completa (ej. fondos)
void fbCompositeFullscreen(FrameBuffer* fb, const Uint32* img, int yStart, int yEnd);

#endif