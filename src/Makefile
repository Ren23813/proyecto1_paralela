# Makefile - Festival Chino (screensaver secuencial vs paralelo)
#
# Uso:
#   make secuencial              -> compila el binario "secuencial"
#   make paralelo                -> compila el binario "paralelo"
#   make                         -> compila ambos
#   make run-secuencial N=2000        -> compila (si hace falta) y corre
#   make run-paralelo N=2000 T=8      -> idem, con T hilos de OpenMP
#   make clean                   -> borra binarios y objetos
#
# Los .o de cada version se guardan en carpetas separadas
# (build-secuencial/ y build-paralelo/) aunque vengan del mismo .c
# (dragon.c, firework.c, lantern.c, framebuffer.c se comparten como
# CODIGO FUENTE pero no como objetos compilados): dragon.c y firework.c
# tienen un "#pragma omp critical" en el generador de numeros aleatorios,
# que solo se respeta de verdad si se compila con -fopenmp. Si los dos
# binarios reutilizaran el mismo dragon.o, el que se compilo primero
# "ganaria" y el otro binario quedaria con esa seccion critica ignorada
# silenciosamente -- carpetas separadas evitan ese problema de raiz.

CC            := gcc
CFLAGS_COMMON := -O2 -Wall -Wextra $(shell pkg-config --cflags sdl2 SDL2_image)
LDLIBS        := $(shell pkg-config --libs sdl2 SDL2_image) -lm

SEQ_BIN := secuencial
PAR_BIN := paralelo

COMMON_SRC := dragon.c firework.c lantern.c framebuffer.c background.c

SEQ_DIR := build-secuencial
PAR_DIR := build-paralelo

SEQ_OBJ := $(SEQ_DIR)/main_secuencial.o $(addprefix $(SEQ_DIR)/,$(COMMON_SRC:.c=.o))
PAR_OBJ := $(PAR_DIR)/main_paralelo.o $(addprefix $(PAR_DIR)/,$(COMMON_SRC:.c=.o))

N ?= 2000
T ?= $(shell nproc)

.PHONY: all clean run-secuencial run-paralelo

all: $(SEQ_BIN) $(PAR_BIN)

# --- Version secuencial: sin -fopenmp ---
$(SEQ_BIN): $(SEQ_OBJ)
	$(CC) $(CFLAGS_COMMON) $^ -o $@ $(LDLIBS)

$(SEQ_DIR)/%.o: %.c | $(SEQ_DIR)
	$(CC) $(CFLAGS_COMMON) -c $< -o $@

# --- Version paralela: con -fopenmp en TODOS sus .o y en el link ---
$(PAR_BIN): $(PAR_OBJ)
	$(CC) $(CFLAGS_COMMON) -fopenmp $^ -o $@ $(LDLIBS)

$(PAR_DIR)/%.o: %.c | $(PAR_DIR)
	$(CC) $(CFLAGS_COMMON) -fopenmp -c $< -o $@

$(SEQ_DIR) $(PAR_DIR):
	mkdir -p $@

run-secuencial: $(SEQ_BIN)
	./$(SEQ_BIN) $(N)

run-paralelo: $(PAR_BIN)
	./$(PAR_BIN) $(N) $(T)

clean:
	rm -rf $(SEQ_DIR) $(PAR_DIR) $(SEQ_BIN) $(PAR_BIN)