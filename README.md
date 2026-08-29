# Proyecto1_paralela: Festival chino

### Idealmente correrlo en una distribución de Linux o WSL. 
Se requiere: `sudo apt install build-essential libsdl2-dev libsdl2-gfx-dev libsdl2-ttf-dev libsdl2-image-dev`
Y se compila (este avance) de esta manera en línea de comando: `gcc src/main.c src/lantern.c -o screensaver `sdl2-config --cflags --libs` -lSDL2_gfx -lm`

### Avance 2
Se implementaron fuegos artificales como elemento adicional.

Estos se encuentran en el archivo de firework.h firework.c y se pueden vizualizar en el archivo main_fuegos.c

Se compila así:

gcc src/main_fuegos.c src/firework.c -o screensaver `sdl2-config --cflags --libs` -lSDL2_gfx -lm

Se ejecuta de la siguiente manera:

./screensaver [n] , donde [n] es la cantidad de 