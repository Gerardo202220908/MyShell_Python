/* productor_consumidor.c -- CoreOS-IF0702 | Entrega 2: IPC y Concurrencia
 * Bounded-Buffer con pthreads + semaforos POSIX.
 *
 * Compilar: gcc -Wall -pthread productor_consumidor.c -o productor_consumidor
 * Ejecutar: ./productor_consumidor [normal|prod-rapido|cons-rapido]
 *   normal      : retardos similares en ambos lados (por defecto)
 *   prod-rapido : productores sin retardo -> el buffer llega a 5/5 y los productores se bloquean
 *   cons-rapido : consumidores sin retardo -> el buffer llega a 0/5 y los consumidores se bloquean
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define TAMANIO_BUFFER        5
#define NUM_PRODUCTORES       2
#define NUM_CONSUMIDORES      3
#define ITEMS_POR_PRODUCTOR   6
#define ITEMS_POR_CONSUMIDOR  4

static int buffer[TAMANIO_BUFFER];
static int cabeza = 0;
static int cola   = 0;
static int count  = 0;

static sem_t mutex;
static sem_t vacios;
static sem_t llenos;

/* Modo de prueba: 0 normal, 1 productores rapidos, 2 consumidores rapidos */
static int modo = 0;

static void retardo_aleatorio(unsigned int *seed) {
    int ms = 50 + (rand_r(seed) % 200);
    struct timespec ts = { .tv_sec = 0, .tv_nsec = ms * 1000000L };
    nanosleep(&ts, NULL);
}

void *productor(void *arg) {
    int id = *(int *)arg;
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)(id * 1103515245u);

    for (int i = 0; i < ITEMS_POR_PRODUCTOR; i++) {
        int item = id * 100 + i;
        if (modo != 1) retardo_aleatorio(&seed);

        sem_wait(&vacios);   /* primero el semaforo de conteo */
        sem_wait(&mutex);    /* luego el mutex */

        buffer[cola] = item;
        cola = (cola + 1) % TAMANIO_BUFFER;
        count++;
        assert(count >= 0 && count <= TAMANIO_BUFFER);

        printf("[PRODUCTOR-%d] Inserto item %3d | buffer: %d/%d\n",
               id, item, count, TAMANIO_BUFFER);
        fflush(stdout);

        sem_post(&mutex);
        sem_post(&llenos);
    }
    return NULL;
}

void *consumidor(void *arg) {
    int id = *(int *)arg;
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)(0xA5A5u + id * 12347u);

    for (int i = 0; i < ITEMS_POR_CONSUMIDOR; i++) {
        if (modo != 2) retardo_aleatorio(&seed);

        sem_wait(&llenos);
        sem_wait(&mutex);

        int item = buffer[cabeza];
        cabeza = (cabeza + 1) % TAMANIO_BUFFER;
        count--;
        assert(count >= 0 && count <= TAMANIO_BUFFER);

        printf("[CONSUMIDOR-%d] Extrajo item %3d | buffer: %d/%d\n",
               id, item, count, TAMANIO_BUFFER);
        fflush(stdout);

        sem_post(&mutex);
        sem_post(&vacios);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    pthread_t h_productores[NUM_PRODUCTORES];
    pthread_t h_consumidores[NUM_CONSUMIDORES];
    int ids_prod[NUM_PRODUCTORES];
    int ids_cons[NUM_CONSUMIDORES];
    int rc;

    if (argc > 1) {
        if (strcmp(argv[1], "prod-rapido") == 0) modo = 1;
        else if (strcmp(argv[1], "cons-rapido") == 0) modo = 2;
        else if (strcmp(argv[1], "normal") != 0) {
            fprintf(stderr, "Uso: %s [normal|prod-rapido|cons-rapido]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    printf("==========================================================\n");
    printf("  Productor-Consumidor -- Buffer Acotado POSIX\n");
    printf("  Productores: %d | Consumidores: %d | Buffer: %d ranuras\n",
           NUM_PRODUCTORES, NUM_CONSUMIDORES, TAMANIO_BUFFER);
    printf("  Modo: %s\n", modo == 1 ? "prod-rapido" : modo == 2 ? "cons-rapido" : "normal");
    printf("==========================================================\n\n");

    if (sem_init(&mutex, 0, 1) != 0 ||
        sem_init(&vacios, 0, TAMANIO_BUFFER) != 0 ||
        sem_init(&llenos, 0, 0) != 0) {
        perror("Error en sem_init");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < NUM_PRODUCTORES; i++) {
        ids_prod[i] = i + 1;
        rc = pthread_create(&h_productores[i], NULL, productor, &ids_prod[i]);
        if (rc != 0) { fprintf(stderr, "Error creando productor: %s\n", strerror(rc)); return EXIT_FAILURE; }
    }
    for (int i = 0; i < NUM_CONSUMIDORES; i++) {
        ids_cons[i] = i + 1;
        rc = pthread_create(&h_consumidores[i], NULL, consumidor, &ids_cons[i]);
        if (rc != 0) { fprintf(stderr, "Error creando consumidor: %s\n", strerror(rc)); return EXIT_FAILURE; }
    }
    for (int i = 0; i < NUM_PRODUCTORES; i++) {
        rc = pthread_join(h_productores[i], NULL);
        if (rc != 0) { fprintf(stderr, "Error en pthread_join productor: %s\n", strerror(rc)); return EXIT_FAILURE; }
    }
    for (int i = 0; i < NUM_CONSUMIDORES; i++) {
        rc = pthread_join(h_consumidores[i], NULL);
        if (rc != 0) { fprintf(stderr, "Error en pthread_join consumidor: %s\n", strerror(rc)); return EXIT_FAILURE; }
    }

    sem_destroy(&mutex);
    sem_destroy(&vacios);
    sem_destroy(&llenos);

    printf("\n==========================================================\n");
    printf("  Ejecucion completada.\n");
    printf("  Items producidos:  %d\n", NUM_PRODUCTORES * ITEMS_POR_PRODUCTOR);
    printf("  Items consumidos:  %d\n", NUM_CONSUMIDORES * ITEMS_POR_CONSUMIDOR);
    printf("  Estado del buffer: %d elemento(s) restante(s)\n", count);
    printf("==========================================================\n");
    return EXIT_SUCCESS;
}