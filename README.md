# Festival Chino — Screensaver (secuencial vs. paralelo)

Screensaver en C + SDL2 que dibuja un festival nocturno chino: dragones
serpenteantes, fuegos artificiales y faroles flotando, sobre un fondo
ilustrado. Existen dos
versiones del mismo programa, secuencial y paralela (OpenMP), para
comparar su rendimiento (FPS) bajo la misma carga de trabajo.


## Requisitos

- GCC con soporte de C11 y OpenMP (`-fopenmp`)
- SDL2 (`libsdl2-dev`)
- SDL2_image (`libsdl2-image-dev`) — para cargar el fondo PNG

En Ubuntu/Debian:

```bash
sudo apt install libsdl2-dev libsdl2-image-dev
```

## Compilar
Dentro de la carpeta de `src/`
```bash
make secuencial      # compila solo el binario "secuencial"
make paralelo        # compila solo el binario "paralelo"
make                 # compila ambos
make clean           # borra binarios y objetos
```

Los `.o` de cada versión se guardan en carpetas separadas
(`build-secuencial/`, `build-paralelo/`) aunque compartan el mismo código
fuente (`dragon.c`, `firework.c`, `lantern.c`, `framebuffer.c`,
`background.c`), porque solo la versión paralela se compila con
`-fopenmp` — así nunca se reutiliza por error un `.o` compilado con las
flags equivocadas entre un binario y otro.

## Correr

```bash
./secuencial [N]
./paralelo [N] [num_hilos]
```

- `N`: cantidad total de elementos a renderizar. Se reparte
  proporcionalmente entre dragones (45%), fuegos artificiales (40%) y
  faroles (15%).
- `num_hilos` (solo en la versión paralela): cantidad de hilos de OpenMP a
  usar. Si se omite, usa el máximo disponible en la máquina.
- `ESC` o cerrar la ventana termina el programa.
- El título de la ventana muestra el FPS actual en vivo.



El archivo `fondo.png` debe estar en el mismo directorio desde donde se
ejecuta el binario (ruta relativa `"fondo.png"`, configurable en
`BACKGROUND_IMAGE_PATH` al inicio de cada `main_*.c`). Si no se encuentra,
el programa sigue funcionando sin fondo (no es un error fatal, pero pues, se ve más feo :D).

## Cómo está organizado el render

`SDL_Renderer` no es *thread-safe*: no se puede dibujar desde varios
hilos a la vez sobre el mismo renderer. Por eso, en vez de dibujar con
llamadas de SDL por elemento, todo (fondo, dragones, fuegos artificiales,
faroles) se dibuja "a mano" sobre un framebuffer: un arreglo plano de
pixeles en RAM (`framebuffer.h`/`.c`). Al final de cada frame, ese arreglo
se sube a pantalla con una sola `SDL_UpdateTexture` + `SDL_RenderCopy`.

Para paralelizar el llenado del framebuffer sin necesitar locks, la
pantalla se reparte en bandas horizontales de filas disjuntas
(`main_paralelo.c`, con `#pragma omp parallel for schedule(dynamic)`).
Cada hilo dibuja una banda completa: recorre todos los elementos, ya
ordenados por profundidad para el efecto de capas correcto, pero solo
pinta los pixeles que caen en su rango de filas. Como las bandas nunca se
superponen, dos hilos jamás escriben el mismo pixel.

La versión secuencial usa exactamente el mismo algoritmo de dibujo, solo
que con una única "banda" que cubre toda la pantalla (como una manta) y sin hilos. De esta forma la
diferencia de FPS entre ambas versiones se puede atribuye a un efecto directo del
paralelismo, no a un algoritmo distinto.


## Estructura de archivos

| Archivo | Contenido |
|---|---|
| `main_secuencial.c` / `main_paralelo.c` | Punto de entrada, loop principal, reparto de `N`, orquestación del render |
| `framebuffer.h` / `.c` | Buffer de pixeles en RAM y primitivas de dibujo (círculo, línea, polígono, rombo con degradado, composición de imagen, etc) |
| `elements.h` | Structs de datos: `Dragon`, `Segment`, `Firework`, `Particle`, `Lantern` |
| `dragon.c` / `.h` | Movimiento, cuerpo (rombos) y cabeza del dragón |
| `firework.c` / `.h` | Física y dibujo de los fuegos artificiales |
| `lantern.c` / `.h` | Movimiento y dibujo de los faroles |
| `background.c` / `.h` | Carga del PNG de fondo a RAM (vía SDL_image) |
| `fondo.png` | Imagen de fondo (transparente, misma resolución que la ventana: 1920×1080) |
| `Makefile` | Build de ambas versiones |
