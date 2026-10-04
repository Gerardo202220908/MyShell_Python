/* ============================================================
 *  productor_consumidor.c
 *  Proyecto CoreOS — IF0702 | Entrega 2: IPC y Concurrencia
 *  Universidad Ricardo Palma — Semestre 2026-II
 *
 *  Implementa el patron Bounded-Buffer (Buffer Acotado) con:
 *   - Multihilo POSIX     (pthread_create / pthread_join con verificacion de errores)
 *   - Exclusion Mutua     (sem_init / sem_wait / sem_post como mutex binario)
 *   - Sin Interbloqueo    (orden estricto: semaforo de conteo ANTES de mutex)
 *   - Invariantes         (assert para garantia formal de no corrupcion del buffer)
 *
 *  Compilacion:
 *      gcc -Wall -pthread productor_consumidor.c -o productor_consumidor
 *
 *  Ejecucion:
 *      ./productor_consumidor
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>     /* strerror() para reporte de errores en pthreads */
#include <assert.h>     /* assert() para verificacion formal de invariantes */
#include <pthread.h>    /* PILAR 1: Multihilo POSIX                  */
#include <semaphore.h>  /* PILAR 2: Semaforos POSIX (exclusion mutua) */
#include <unistd.h>     /* usleep()                                   */
#include <time.h>       /* nanosleep() para delays aleatorios         */

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
/*  BUFFER CIRCULAR COMPARTIDO (Seccion Critica)               */
/* ─────────────────────────────────────────────────────────── */
static int buffer[TAMANIO_BUFFER];  /* Almacenamiento circular        */
static int cabeza = 0;              /* Indice de lectura (consumidor) */
static int cola   = 0;              /* Indice de escritura (productor)*/
static int count  = 0;              /* Elementos actuales en el buffer */

/* ─────────────────────────────────────────────────────────── */
/*  PRIMITIVAS DE SINCRONIZACION                               */
/* ─────────────────────────────────────────────────────────── */

/* PILAR 2 — Mutex binario: garantiza exclusion mutua sobre el buffer */
static sem_t mutex;

/* Semaforo de conteo: indica cuantas ranuras VACIAS hay en el buffer */
static sem_t vacios;

/* Semaforo de conteo: indica cuantas ranuras LLENAS hay en el buffer */
static sem_t llenos;

/* ─────────────────────────────────────────────────────────── */
/*  FUNCION AUXILIAR: retardo aleatorio entre 50-250 ms        */
/* ─────────────────────────────────────────────────────────── */
static void retardo_aleatorio(void) {
    int ms = 50 + (rand() % 200);
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

    for (int i = 0; i < ITEMS_POR_PRODUCTOR; i++) {
        int item = id * 100 + i;   /* Item unico identificable por productor */

        retardo_aleatorio();       /* Simula tiempo de produccion            */

        /* ── PILAR 3: Orden correcto → evita deadlock ─────────────────
         *  1. sem_wait(&vacios):  bloquea si el buffer esta LLENO.
         *     El hilo espera aqui sin tener el mutex, por lo que
         *     otros hilos pueden continuar operando libremente.
         *  2. sem_wait(&mutex):  adquiere el cerrojo de exclusion mutua
         *     SOLO cuando ya hay espacio garantizado en el buffer.
         * ──────────────────────────────────────────────────────────── */
        
        sem_wait(&vacios);         /* Espera ranura vacia (conteo)           */
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

    for (int i = 0; i < ITEMS_POR_CONSUMIDOR; i++) {
        retardo_aleatorio();       /* Simula tiempo de procesamiento         */

        /* ── PILAR 3: Orden correcto → evita deadlock ─────────────────
         *  1. sem_wait(&llenos): bloquea si el buffer esta VACIO.
         *     El hilo cede la CPU sin retener el mutex.
         *  2. sem_wait(&mutex):  adquiere el cerrojo solo cuando hay
         *     un item garantizado para consumir.
         * ──────────────────────────────────────────────────────────── */
        sem_wait(&llenos);         /* Espera ranura llena (conteo)           */
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
int main(void) {
    srand((unsigned int)time(NULL));

    printf("==========================================================\n");
    printf("  Productor-Consumidor -- Buffer Acotado POSIX            \n");
    printf("  Productores: %d | Consumidores: %d | Buffer: %d ranuras \n",
           NUM_PRODUCTORES, NUM_CONSUMIDORES, TAMANIO_BUFFER);
    printf("==========================================================\n\n");

    /* ── PILAR 2: Inicializacion de semaforos con verificacion de errores ──
     *  sem_init(&sem, pshared, valor_inicial)
     *   - pshared = 0  → compartido entre hilos del mismo proceso
     *   - mutex   = 1  → cerrojo libre al inicio
     *   - vacios  = N  → todas las ranuras vacias al inicio
     *   - llenos  = 0  → ninguna ranura llena al arrancar
     * ──────────────────────────────────────────────────────────────── */
    if (sem_init(&mutex, 0, 1) != 0) {
        perror("Error critico: sem_init(mutex) fallo");
        exit(EXIT_FAILURE);
    }
    if (sem_init(&vacios, 0, TAMANIO_BUFFER) != 0) {
        perror("Error critico: sem_init(vacios) fallo");
        exit(EXIT_FAILURE);
    }
    if (sem_init(&llenos, 0, 0) != 0) {
        perror("Error critico: sem_init(llenos) fallo");
        exit(EXIT_FAILURE);
    }

    /* ── PILAR 1: Arrays de identificadores de hilo ──────────────────── */
    pthread_t h_productores[NUM_PRODUCTORES];
    pthread_t h_consumidores[NUM_CONSUMIDORES];
    int ids_prod[NUM_PRODUCTORES];
    int ids_cons[NUM_CONSUMIDORES];

    /* ── PILAR 1: Creacion de hilos productores con verificacion de error */
    for (int i = 0; i < NUM_PRODUCTORES; i++) {
        ids_prod[i] = i + 1;
        int rc = pthread_create(&h_productores[i], NULL, productor, &ids_prod[i]);
        if (rc != 0) {
            fprintf(stderr, "Error al crear hilo productor %d: %s\n", ids_prod[i], strerror(rc));
            exit(EXIT_FAILURE);
        }
    }

    /* ── PILAR 1: Creacion de hilos consumidores con verificacion de error */
    for (int i = 0; i < NUM_CONSUMIDORES; i++) {
        ids_cons[i] = i + 1;
        int rc = pthread_create(&h_consumidores[i], NULL, consumidor, &ids_cons[i]);
        if (rc != 0) {
            fprintf(stderr, "Error al crear hilo consumidor %d: %s\n", ids_cons[i], strerror(rc));
            exit(EXIT_FAILURE);
        }
    }

    /* ── PILAR 1: Espera de terminacion limpia (join) con verificacion ── */
    for (int i = 0; i < NUM_PRODUCTORES; i++) {
        int rc = pthread_join(h_productores[i], NULL);
        if (rc != 0) {
            fprintf(stderr, "Error esperando termino del productor %d: %s\n", i + 1, strerror(rc));
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < NUM_CONSUMIDORES; i++) {
        int rc = pthread_join(h_consumidores[i], NULL);
        if (rc != 0) {
            fprintf(stderr, "Error esperando termino del consumidor %d: %s\n", i + 1, strerror(rc));
            exit(EXIT_FAILURE);
        }
    }

    /* ── Destruccion de semaforos y cierre ───────────────────────────── */
    sem_destroy(&mutex);
    sem_destroy(&vacios);
    sem_destroy(&llenos);

    printf("\n==========================================================\n");
    printf("  Ejecucion completada. Sin Deadlocks. Sin Race Conditions.\n");
    printf("  Items producidos:  %d\n", NUM_PRODUCTORES * ITEMS_POR_PRODUCTOR);
    printf("  Items consumidos:  %d\n", NUM_CONSUMIDORES * ITEMS_POR_CONSUMIDOR);
    printf("  Estado del buffer: %d elemento(s) restante(s)\n", count);
    printf("==========================================================\n");

    return 0;
}