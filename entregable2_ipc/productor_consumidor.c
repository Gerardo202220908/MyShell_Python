/* ============================================================
 *  productor_consumidor.c
 *  Proyecto CoreOS — IF0702 | Entrega 2: IPC y Concurrencia
 *  Universidad Ricardo Palma — Semestre 2026-II
 *
 *  Implementa el patron Bounded-Buffer (Buffer Acotado) con:
 *   - Multihilo POSIX       (pthread_create / pthread_join con verificacion de errores)
 *   - Exclusion Mutua       (sem_init / sem_wait / sem_post como mutex binario)
 *   - Sin Interbloqueo      (orden estricto: semaforo de conteo ANTES de mutex)
 *   - Invariantes           (assert para garantia formal de no corrupcion del buffer)
 *
 *  Compilacion:
 *      gcc -Wall -pthread productor_consumidor.c -o productor_consumidor
 *
 *  Ejecucion:
 *      ./productor_consumidor [normal|prod-rapido|cons-rapido]
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>     /* strerror() para reporte de errores en pthreads */
#include <assert.h>     /* assert() para verificacion formal de invariantes */
#include <pthread.h>    /* PILAR 1: Multihilo POSIX                 */
#include <semaphore.h>  /* PILAR 2: Semaforos POSIX (exclusion mutua) */
#include <unistd.h>     /* usleep()                                 */
#include <time.h>       /* nanosleep() para delays aleatorios       */

/* ─────────────────────────────────────────────────────────── */
/*  PARAMETROS DEL BUFFER ACOTADO                              */
/* ─────────────────────────────────────────────────────────── */
#define TAMANIO_BUFFER       5   /* Capacidad maxima del buffer circular  */
#define NUM_PRODUCTORES      2   /* Numero de hilos productores           */
#define NUM_CONSUMIDORES     3   /* Numero de hilos consumidores          */

/* Balance de ítems: 2 prod * 6 = 12 total | 3 cons * 4 = 12 total */
#define ITEMS_POR_PRODUCTOR  6   /* Items que produce cada productor      */
#define ITEMS_POR_CONSUMIDOR 4   /* Items que consume cada consumidor     */

/* ─────────────────────────────────────────────────────────── */
/*  BUFFER CIRCULAR COMPARTIDO (Seccion Critica)                */
/* ─────────────────────────────────────────────────────────── */
static int buffer[TAMANIO_BUFFER];  /* Almacenamiento circular        */
static int cabeza = 0;              /* Indice de lectura (consumidor) */
static int cola   = 0;              /* Indice de escritura (productor)*/
static int count  = 0;              /* Elementos actuales en el buffer */

/* ─────────────────────────────────────────────────────────── */
/*  PRIMITIVAS DE SINCRONIZACION                               */
/* ─────────────────────────────────────────────────────────── */
static sem_t mutex;
static sem_t vacios;
static sem_t llenos;

/* Modo de prueba: 0 normal, 1 productores rapidos, 2 consumidores rapidos */
static int modo = 0;

/* ─────────────────────────────────────────────────────────── */
/*  FUNCION AUXILIAR: retardo aleatorio entre 50-250 ms        */
/* ─────────────────────────────────────────────────────────── */
static void retardo_aleatorio(unsigned int *seed) {
    int ms = 50 + (rand_r(seed) % 200);
    struct timespec ts = {
        .tv_sec  = 0,
        .tv_nsec = ms * 1000000L
    };
    nanosleep(&ts, NULL);
}

/* ─────────────────────────────────────────────────────────── */
/*  HILO PRODUCTOR                                             */
/* ─────────────────────────────────────────────────────────── */
void *productor(void *arg) {
    int id = *(int *)arg;
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)(id * 1103515245u);

    for (int i = 0; i < ITEMS_POR_PRODUCTOR; i++) {
        int item = id * 100 + i;   /* Item unico identificable por productor */

        if (modo != 1) retardo_aleatorio(&seed);  /* Simula tiempo de produccion */

        /* ── PILAR 3: Orden correcto → evita deadlock ─────────────────
         *  1. sem_wait(&vacios):   bloquea si el buffer esta LLENO.
         *  2. sem_wait(&mutex):    adquiere el cerrojo de exclusion mutua
         * ──────────────────────────────────────────────────────────── */
        sem_wait(&vacios);         /* Espera ranura vacia (conteo)          */
        sem_wait(&mutex);          /* Adquiere el mutex binario (PILAR 2)    */

        /* == INICIO SECCION CRITICA ====================================*/
        buffer[cola] = item;
        cola         = (cola + 1) % TAMANIO_BUFFER;
        count++;

        /* Verificacion de invariante formal: no sobrellenado del buffer */
        assert(count >= 0 && count <= TAMANIO_BUFFER);

        printf("[PRODUCTOR-%d] Inserto item %3d  | buffer: %d/%d\n",
                id, item, count, TAMANIO_BUFFER);
        fflush(stdout);
        /* == FIN SECCION CRITICA ========================================*/

        sem_post(&mutex);          /* Libera el mutex binario                */
        sem_post(&llenos);         /* Senaliza que hay una ranura llena mas  */
    }

    return NULL;
}

/* ─────────────────────────────────────────────────────────── */
/*  HILO CONSUMIDOR                                            */
/* ─────────────────────────────────────────────────────────── */
void *consumidor(void *arg) {
    int id = *(int *)arg;
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)(0xA5A5u + id * 12347u);

    for (int i = 0; i < ITEMS_POR_CONSUMIDOR; i++) {
        if (modo != 2) retardo_aleatorio(&seed);  /* Simula tiempo de procesamiento */

        /* ── PILAR 3: Orden correcto → evita deadlock ─────────────────
         *  1. sem_wait(&llenos): bloquea si el buffer esta VACIO.
         *  2. sem_wait(&mutex):  adquiere el cerrojo solo cuando hay item.
         * ──────────────────────────────────────────────────────────── */
        sem_wait(&llenos);         /* Espera ranura llena (conteo)          */
        sem_wait(&mutex);          /* Adquiere el mutex binario (PILAR 2)    */

        /* == INICIO SECCION CRITICA ====================================*/
        int item = buffer[cabeza];
        cabeza   = (cabeza + 1) % TAMANIO_BUFFER;
        count--;

        /* Verificacion de invariante formal: no subflujo del buffer */
        assert(count >= 0 && count <= TAMANIO_BUFFER);

        printf("[CONSUMIDOR-%d] Extrajo  item %3d  | buffer: %d/%d\n",
                id, item, count, TAMANIO_BUFFER);
        fflush(stdout);
        /* == FIN SECCION CRITICA ========================================*/

        sem_post(&mutex);          /* Libera el mutex binario                */
        sem_post(&vacios);         /* Senaliza que hay una ranura vacia mas  */
    }

    return NULL;
}

/* ─────────────────────────────────────────────────────────── */
/*  FUNCION PRINCIPAL                                          */
/* ─────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "prod-rapido") == 0) modo = 1;
        else if (strcmp(argv[1], "cons-rapido") == 0) modo = 2;
        else if (strcmp(argv[1], "normal") != 0) {
            fprintf(stderr, "Uso: %s [normal|prod-rapido|cons-rapido]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    printf("==========================================================\n");
    printf("  Productor-Consumidor -- Buffer Acotado POSIX            \n");
    printf("  Productores: %d | Consumidores: %d | Buffer: %d ranuras \n",
           NUM_PRODUCTORES, NUM_CONSUMIDORES, TAMANIO_BUFFER);
    printf("  Modo: %s\n", modo == 1 ? "prod-rapido" : modo == 2 ? "cons-rapido" : "normal");
    printf("==========================================================\n\n");

    if (sem_init(&mutex, 0, 1) != 0 ||
        sem_init(&vacios, 0, TAMANIO_BUFFER) != 0 ||
        sem_init(&llenos, 0, 0) != 0) {
        perror("Error critico en sem_init");
        return EXIT_FAILURE;
    }

    pthread_t h_productores[NUM_PRODUCTORES];
    pthread_t h_consumidores[NUM_CONSUMIDORES];
    int ids_prod[NUM_PRODUCTORES];
    int ids_cons[NUM_CONSUMIDORES];
    int rc;

    for (int i = 0; i < NUM_PRODUCTORES; i++) {
        ids_prod[i] = i + 1;
        rc = pthread_create(&h_productores[i], NULL, productor, &ids_prod[i]);
        if (rc != 0) {
            fprintf(stderr, "Error al crear hilo productor %d: %s\n", ids_prod[i], strerror(rc));
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < NUM_CONSUMIDORES; i++) {
        ids_cons[i] = i + 1;
        rc = pthread_create(&h_consumidores[i], NULL, consumidor, &ids_cons[i]);
        if (rc != 0) {
            fprintf(stderr, "Error al crear hilo consumidor %d: %s\n", ids_cons[i], strerror(rc));
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < NUM_PRODUCTORES; i++) {
        rc = pthread_join(h_productores[i], NULL);
        if (rc != 0) {
            fprintf(stderr, "Error esperando termino del productor %d: %s\n", i + 1, strerror(rc));
            return EXIT_FAILURE;
        }
    }
    for (int i = 0; i < NUM_CONSUMIDORES; i++) {
        rc = pthread_join(h_consumidores[i], NULL);
        if (rc != 0) {
            fprintf(stderr, "Error esperando termino del consumidor %d: %s\n", i + 1, strerror(rc));
            return EXIT_FAILURE;
        }
    }

    sem_destroy(&mutex);
    sem_destroy(&vacios);
    sem_destroy(&llenos);

    printf("\n==========================================================\n");
    printf("  Ejecucion completada. Sin Deadlocks. Sin Race Conditions.\n");
    printf("  Items producidos:  %d\n", NUM_PRODUCTORES * ITEMS_POR_PRODUCTOR);
    printf("  Items consumidos:  %d\n", NUM_CONSUMIDORES * ITEMS_POR_CONSUMIDOR);
    printf("  Estado del buffer: %d elemento(s) restante(s)\n", count);
    printf("==========================================================\n");

    return EXIT_SUCCESS;
}