CC = gcc
SRC_COMMON = src/lantern.c src/firework.c src/dragon.c
CFLAGS_SDL = $(shell sdl2-config --cflags)
LDFLAGS_SDL = $(shell sdl2-config --libs) -lSDL2_gfx -lm

.PHONY: all secuencial paralelo clean

all: secuencial paralelo

secuencial: src/main_secuencial.c $(SRC_COMMON)
	$(CC) src/main_secuencial.c $(SRC_COMMON) -o screensaver_secuencial $(CFLAGS_SDL) $(LDFLAGS_SDL)

paralelo: src/main_paralelo.c $(SRC_COMMON)
	$(CC) src/main_paralelo.c $(SRC_COMMON) -o screensaver_paralelo $(CFLAGS_SDL) -fopenmp $(LDFLAGS_SDL) -fopenmp

clean:
	rm -f screensaver_secuencial screensaver_paralelo