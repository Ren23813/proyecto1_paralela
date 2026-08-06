#include <stdio.h>
#include <omp.h>

int main() {
    #ifdef _OPENMP
        printf("¡OpenMP esta configurado correctamente!\n");
        printf("Version de OpenMP (AAAAMM): %d\n", _OPENMP);
        printf("Hilos disponibles: %d\n", omp_get_max_threads());
    #else
        printf("OpenMP NO esta habilitado en este compilador.\n");
    #endif
    return 0;
}
