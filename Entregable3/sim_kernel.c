/*
 * sim_kernel.c  -  Entrega 3 (IF 0702 - Sistemas Operativos)
 * Planificador de CPU (Round Robin apropiativo / SJF no apropiativo)
 * acoplado a un Administrador de Memoria Virtual Paginada
 * (reemplazo GLOBAL: FIFO, LRU, Reloj) con deteccion de thrashing.
 *
 * Modelo de tiempo (unidades de simulacion, 1 u = 1 ms):
 *   - Cada referencia a memoria consume 1 u de CPU util.
 *   - Un fallo de pagina agrega PF_PENALTY u de E/S de disco (CPU detenida).
 *   - Cambiar de un proceso a otro cuesta CS_COSTO u (conmutacion de contexto).
 *
 * Deteccion de thrashing:
 *   - Ventana deslizante de VENTANA_THR referencias.
 *   - Si la tasa de fallos dentro de la ventana >= UMBRAL_THRASHING,
 *     se considera que el sistema esta en thrashing.
 *   - Se exportan PctThrashing (fraccion de referencias en thrashing)
 *     y EventosThrashing (transiciones hacia thrashing).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_PROCESOS     10
#define TAM_PAGINA       4096
#define MAX_PAGINAS_PROC 64
#define MAX_REFS         256
#define PF_PENALTY       5
#define CS_COSTO         1
#define UMBRAL_THRASHING 0.50   /* tasa de fallos >= 50% => thrashing */
#define VENTANA_THR      20     /* ventana deslizante de referencias */
#define COLA_MAX         4096

typedef enum { FIFO, LRU, RELOJ } AlgoReemplazo;
typedef enum { PLAN_RR, PLAN_SJF } Planificador;

static const char *NOMBRE_ALGO[] = { "FIFO", "LRU", "RELOJ" };
static const char *NOMBRE_PLAN[] = { "RR", "SJF" };

typedef struct {
    int  marco_fisico;
    bool presente;       /* bit de validez/presencia */
    bool referenciado;   /* bit R */
    int  ultimo_acceso;
} EntradaTablaPagina;

typedef struct {
    int pid, tiempo_llegada, rafaga_cpu, tiempo_restante;
    int tiempo_inicio, tiempo_fin, tiempo_espera, tiempo_retorno;
    int referencias_memoria[MAX_REFS];
    int num_referencias;
    EntradaTablaPagina tabla_paginas[MAX_PAGINAS_PROC];
    bool paginas_vistas[MAX_PAGINAS_PROC];  /* para fallos obligatorios */
    int page_faults;
    int pf_obligatorios;
    int pf_capacidad;
} Proceso;

typedef struct {
    int  pid_propietario, num_pagina;
    bool ocupado, referenciado;
    int  tiempo_carga;     /* para FIFO */
    int  ultimo_acceso;    /* para LRU  */
} MarcoRAM;

typedef struct {
    double avg_wt, avg_tat, avg_rt, tasa_fallos, util_cpu;
    double pct_thrashing;
    int total_pf, cambios_ctx, tiempo_total;
    int pf_obligatorios, pf_capacidad;
    int eventos_thrashing;
} Resultado;

/* ---------- estado global del simulador ---------- */
static int num_marcos_ram = 8;
static MarcoRAM *ram = NULL;
static AlgoReemplazo algoritmo_reemplazo = LRU;
static int manecilla_reloj = 0;
static int contador_tiempo_global = 0;
static Proceso *procesos_globales = NULL;
static int total_procesos_globales = 0;
static FILE *traza = NULL;

/* ---------- detector de thrashing (ventana deslizante) ---------- */
static unsigned char ventana[VENTANA_THR];
static int vpos = 0, vcount = 0, vfallos = 0;
static int en_thrashing = 0, eventos_thrashing = 0, refs_thrashing = 0;

static void reiniciar_ventana(void) {
    memset(ventana, 0, sizeof ventana);
    vpos = vcount = vfallos = 0;
    en_thrashing = eventos_thrashing = refs_thrashing = 0;
}

/* Se llama UNA VEZ POR REFERENCIA (hit o fallo), nunca solo en fallos. */
static void registrar_ref(int fallo) {
    if (vcount == VENTANA_THR) {
        vfallos -= ventana[vpos];
    } else {
        vcount++;
    }
    ventana[vpos] = (unsigned char)(fallo ? 1 : 0);
    vfallos += fallo ? 1 : 0;
    vpos = (vpos + 1) % VENTANA_THR;

    int thr = (vcount == VENTANA_THR) &&
            ((double)vfallos / VENTANA_THR >= UMBRAL_THRASHING);

    if (thr && !en_thrashing) {
        eventos_thrashing++;
        if (traza)
            fprintf(traza, "--- THRASHING detectado en ref=%d ---\n",
                    contador_tiempo_global);
    }
    en_thrashing = thr;
    if (thr) refs_thrashing++;
}

static Proceso *buscar_proceso(int pid) {
    for (int k = 0; k < total_procesos_globales; k++)
        if (procesos_globales[k].pid == pid) return &procesos_globales[k];
    return NULL;
}

/* ---------- seleccion de victima ---------- */
static int elegir_victima(void) {
    int v = 0;
    if (algoritmo_reemplazo == FIFO) {
        for (int i = 1; i < num_marcos_ram; i++)
            if (ram[i].tiempo_carga < ram[v].tiempo_carga) v = i;
    } else if (algoritmo_reemplazo == LRU) {
        for (int i = 1; i < num_marcos_ram; i++)
            if (ram[i].ultimo_acceso < ram[v].ultimo_acceso) v = i;
    } else { /* RELOJ: segunda oportunidad */
        while (1) {
            MarcoRAM *f = &ram[manecilla_reloj];
            if (f->referenciado) {
                f->referenciado = false;
                Proceso *o = buscar_proceso(f->pid_propietario);
                if (o) o->tabla_paginas[f->num_pagina].referenciado = false;
                manecilla_reloj = (manecilla_reloj + 1) % num_marcos_ram;
            } else {
                v = manecilla_reloj;
                manecilla_reloj = (manecilla_reloj + 1) % num_marcos_ram;
                break;
            }
        }
    }
    return v;
}

/* ---------- traduccion virtual -> fisica ----------
 * dir_virtual = [ numero de pagina | desplazamiento (12 bits) ]
 * dir_fisica  = marco * TAM_PAGINA + desplazamiento
 * Devuelve 1 si hubo fallo de pagina, 0 si fue acierto. */
static int acceder_memoria(Proceso *p, int dir_virtual, int *dir_fisica) {
    int page_number = dir_virtual / TAM_PAGINA;
    int offset      = dir_virtual % TAM_PAGINA;
    if (page_number >= MAX_PAGINAS_PROC) page_number = MAX_PAGINAS_PROC - 1;

    contador_tiempo_global++;
    EntradaTablaPagina *pte = &p->tabla_paginas[page_number];

    if (pte->presente) {                                  /* HIT */
        pte->referenciado = true;
        pte->ultimo_acceso = contador_tiempo_global;
        ram[pte->marco_fisico].referenciado = true;
        ram[pte->marco_fisico].ultimo_acceso = contador_tiempo_global;
        p->paginas_vistas[page_number] = true;
        *dir_fisica = pte->marco_fisico * TAM_PAGINA + offset;
        if (traza) fprintf(traza, "%4d | P%d | 0x%06X | pag %2d | off %4d | marco %2d | 0x%06X | HIT\n",
            contador_tiempo_global, p->pid, dir_virtual, page_number, offset,
            pte->marco_fisico, *dir_fisica);
        return 0;
    }

    p->page_faults++;                                     /* FALLO */
    if (!p->paginas_vistas[page_number]) {
        p->paginas_vistas[page_number] = true;
        p->pf_obligatorios++;
    } else {
        p->pf_capacidad++;
    }

    int marco = -1;
    for (int i = 0; i < num_marcos_ram; i++)
        if (!ram[i].ocupado) { marco = i; break; }

    int vpid = -1, vpag = -1;
    if (marco == -1) {
        marco = elegir_victima();
        vpid = ram[marco].pid_propietario;
        vpag = ram[marco].num_pagina;
        Proceso *o = buscar_proceso(vpid);
        if (o) { o->tabla_paginas[vpag].presente = false;
                o->tabla_paginas[vpag].marco_fisico = -1;
                o->tabla_paginas[vpag].referenciado = false; }
    }
    ram[marco].ocupado = true;
    ram[marco].pid_propietario = p->pid;
    ram[marco].num_pagina = page_number;
    ram[marco].referenciado = true;
    ram[marco].tiempo_carga = contador_tiempo_global;
    ram[marco].ultimo_acceso = contador_tiempo_global;

    pte->presente = true;
    pte->marco_fisico = marco;
    pte->referenciado = true;
    pte->ultimo_acceso = contador_tiempo_global;

    *dir_fisica = marco * TAM_PAGINA + offset;
    if (traza) {
        fprintf(traza, "%4d | P%d | 0x%06X | pag %2d | off %4d | marco %2d | 0x%06X | FALLO",
            contador_tiempo_global, p->pid, dir_virtual, page_number, offset, marco, *dir_fisica);
        if (vpid != -1) fprintf(traza, " (victima: P%d pag %d)", vpid, vpag);
        fprintf(traza, "\n");
    }
    return 1;
}

/* ---------- ejecucion de 'ticks' unidades de CPU de un proceso ---------- */
static void ejecutar(Proceso *p, int ticks, int *reloj, int *pf_total) {
    for (int t = 0; t < ticks; t++) {
        int idx = (p->rafaga_cpu - p->tiempo_restante + t) % p->num_referencias;
        int dfis;
        (*reloj)++;                                   /* 1 u de CPU util */
        int fallo = acceder_memoria(p, p->referencias_memoria[idx], &dfis);
        registrar_ref(fallo);                         /* ventana de thrashing */
        if (fallo) {
            (*pf_total)++;
            *reloj += PF_PENALTY;                     /* E/S de disco */
        }
    }
    p->tiempo_restante -= ticks;
}

static void finalizar(Proceso *p, int reloj) {
    p->tiempo_fin = reloj;
    p->tiempo_retorno = p->tiempo_fin - p->tiempo_llegada;
    p->tiempo_espera  = p->tiempo_retorno - p->rafaga_cpu;
}

static void conmutar(int *ultimo_pid, int pid, int *reloj, int *cs, FILE *log) {
    if (*ultimo_pid != pid) {
        if (*ultimo_pid != -1) { *reloj += CS_COSTO; (*cs)++; }
        if (log) fprintf(log, "t=%4d  despacho -> P%d\n", *reloj, pid);
        *ultimo_pid = pid;
    }
}

static Resultado calcular(Proceso procesos[], int n, int reloj, int pf, int cs) {
    Resultado r; memset(&r, 0, sizeof r);
    int refs = 0, cpu = 0, pf_oblig = 0, pf_cap = 0;
    for (int i = 0; i < n; i++) {
        r.avg_wt  += procesos[i].tiempo_espera;
        r.avg_tat += procesos[i].tiempo_retorno;
        r.avg_rt  += procesos[i].tiempo_inicio - procesos[i].tiempo_llegada;
        refs += procesos[i].rafaga_cpu;
        cpu  += procesos[i].rafaga_cpu;
        pf_oblig += procesos[i].pf_obligatorios;
        pf_cap   += procesos[i].pf_capacidad;
    }
    r.avg_wt /= n; r.avg_tat /= n; r.avg_rt /= n;
    r.total_pf = pf; r.cambios_ctx = cs; r.tiempo_total = reloj;
    r.tasa_fallos = (double)pf / refs;
    r.util_cpu = 100.0 * cpu / reloj;
    r.pf_obligatorios = pf_oblig;
    r.pf_capacidad = pf_cap;
    r.pct_thrashing = (double)refs_thrashing / refs;
    r.eventos_thrashing = eventos_thrashing;
    return r;
}

/* ---------- Round Robin (apropiativo) ---------- */
static Resultado simular_round_robin(Proceso procesos[], int n, int quantum, FILE *log) {
    procesos_globales = procesos; total_procesos_globales = n;
    int reloj = 0, completados = 0, pf = 0, cs = 0, ultimo = -1;
    static int cola[COLA_MAX]; int frente = 0, final = 0;
    bool en_cola[MAX_PROCESOS] = { false };

#define ENCOLAR(x) do { \
        if (final >= COLA_MAX) { \
            fprintf(stderr, "Error: cola de listos desbordada\n"); \
            exit(EXIT_FAILURE); \
        } \
        cola[final++] = (x); \
        en_cola[(x)] = true; \
    } while (0)

    for (int i = 0; i < n; i++)
        if (procesos[i].tiempo_llegada <= reloj) ENCOLAR(i);

    while (completados < n) {
        if (frente == final) {                        /* CPU ociosa */
            reloj++;
            for (int i = 0; i < n; i++)
                if (procesos[i].tiempo_llegada <= reloj && !en_cola[i] && procesos[i].tiempo_restante > 0)
                    ENCOLAR(i);
            continue;
        }
        int idx = cola[frente++];
        Proceso *p = &procesos[idx];
        conmutar(&ultimo, p->pid, &reloj, &cs, log);
        if (p->tiempo_inicio == -1) p->tiempo_inicio = reloj;

        int q = (p->tiempo_restante > quantum) ? quantum : p->tiempo_restante;
        ejecutar(p, q, &reloj, &pf);

        for (int i = 0; i < n; i++)                   /* llegadas durante la rodaja */
            if (i != idx && procesos[i].tiempo_llegada <= reloj && !en_cola[i] && procesos[i].tiempo_restante > 0)
                ENCOLAR(i);

        if (p->tiempo_restante > 0) ENCOLAR(idx);
        else { finalizar(p, reloj); completados++; }
    }
#undef ENCOLAR
    return calcular(procesos, n, reloj, pf, cs);
}

/* ---------- SJF (no apropiativo) ---------- */
static Resultado simular_sjf(Proceso procesos[], int n, FILE *log) {
    procesos_globales = procesos; total_procesos_globales = n;
    int reloj = 0, completados = 0, pf = 0, cs = 0, ultimo = -1;
    while (completados < n) {
        int mejor = -1;
        for (int i = 0; i < n; i++)
            if (procesos[i].tiempo_restante > 0 && procesos[i].tiempo_llegada <= reloj)
                if (mejor == -1 || procesos[i].rafaga_cpu < procesos[mejor].rafaga_cpu) mejor = i;
        if (mejor == -1) { reloj++; continue; }
        Proceso *p = &procesos[mejor];
        conmutar(&ultimo, p->pid, &reloj, &cs, log);
        p->tiempo_inicio = reloj;
        ejecutar(p, p->tiempo_restante, &reloj, &pf);
        finalizar(p, reloj); completados++;
    }
    return calcular(procesos, n, reloj, pf, cs);
}

/* ---------- carga de trabajo con localidad (working sets) ---------- */
static unsigned int semilla = 2026;
static int rnd(void) { semilla = semilla * 1103515245u + 12345u; return (semilla >> 16) & 0x7fff; }

/* Dos fases de localidad: WS = {0..ws-1}; en la 2da mitad el WS se desplaza ws/2 paginas.
 * 70% de las referencias recorren el WS en bucle, 30% saltan aleatoriamente dentro de el. */
static void crear_proceso(Proceso *p, int pid, int llegada, int rafaga, int ws) {
    memset(p, 0, sizeof *p);
    p->pid = pid; p->tiempo_llegada = llegada; p->rafaga_cpu = rafaga;
    p->num_referencias = rafaga;
    int seq = 0;
    for (int i = 0; i < rafaga; i++) {
        int base = (i < rafaga / 2) ? 0 : ws / 2;
        int pag = (rnd() % 100 < 70) ? (seq++ % ws) : (rnd() % ws);
        p->referencias_memoria[i] = (base + pag) * TAM_PAGINA + rnd() % TAM_PAGINA;
    }
}

static void reiniciar_procesos(Proceso src[], Proceso dest[], int n) {
    for (int i = 0; i < n; i++) {
        dest[i] = src[i];
        dest[i].tiempo_restante = dest[i].rafaga_cpu;
        dest[i].tiempo_inicio = -1;
        dest[i].tiempo_fin = dest[i].tiempo_espera = dest[i].tiempo_retorno = 0;
        dest[i].page_faults = 0;
        dest[i].pf_obligatorios = 0;
        dest[i].pf_capacidad = 0;
        memset(dest[i].paginas_vistas, 0, sizeof dest[i].paginas_vistas);
        for (int j = 0; j < MAX_PAGINAS_PROC; j++) {
            dest[i].tabla_paginas[j].presente = false;
            dest[i].tabla_paginas[j].marco_fisico = -1;
            dest[i].tabla_paginas[j].referenciado = false;
            dest[i].tabla_paginas[j].ultimo_acceso = 0;
        }
    }
}

static Resultado correr(Proceso base[], int n, Planificador pl, AlgoReemplazo al,
                        int marcos, int quantum, FILE *tr, FILE *log) {
    num_marcos_ram = marcos; algoritmo_reemplazo = al;
    ram = (MarcoRAM *)calloc(marcos, sizeof(MarcoRAM));
    if (!ram) { perror("calloc"); exit(EXIT_FAILURE); }
    manecilla_reloj = 0; contador_tiempo_global = 0; traza = tr;
    reiniciar_ventana();
    Proceso act[MAX_PROCESOS];
    reiniciar_procesos(base, act, n);
    Resultado r = (pl == PLAN_RR) ? simular_round_robin(act, n, quantum, log)
                                : simular_sjf(act, n, log);
    free(ram); traza = NULL;
    return r;
}

static void escribir_fila(FILE *f, Planificador pl, AlgoReemplazo al, int marcos, int q, Resultado r) {
    fprintf(f, "%s,%s,%d,%d,%.2f,%.2f,%.2f,%d,%.4f,%.2f,%d,%d,%d,%d,%.4f,%d\n",
        NOMBRE_ALGO[al], NOMBRE_PLAN[pl], marcos, q, r.avg_wt, r.avg_tat, r.avg_rt,
        r.total_pf, r.tasa_fallos, r.util_cpu, r.cambios_ctx, r.tiempo_total,
        r.pf_obligatorios, r.pf_capacidad, r.pct_thrashing, r.eventos_thrashing);
}
static const char *CABECERA =
    "Algoritmo,Planificador,MarcosRAM,Quantum,AVG_WT,AVG_TAT,AVG_RT,TotalPageFaults,"
    "TasaFallos,UtilCPU,CambiosContexto,TiempoTotal,PF_Obligatorios,PF_Capacidad,"
    "PctThrashing,EventosThrashing\n";

int main(void) {
    Proceso carga[4];
    crear_proceso(&carga[0], 1, 0, 50, 4);
    crear_proceso(&carga[1], 2, 1, 40, 3);
    crear_proceso(&carga[2], 3, 2, 60, 5);
    crear_proceso(&carga[3], 4, 3, 30, 4);
    int n = 4;

    FILE *csv = fopen("reporte_thrashing.csv", "w");
    FILE *csvq = fopen("reporte_quantum.csv", "w");
    FILE *ftr = fopen("traza_traduccion.txt", "w");
    FILE *flog = fopen("traza_planificador.txt", "w");
    if (!csv || !csvq || !ftr || !flog) { perror("fopen"); return 1; }
    fputs(CABECERA, csv); fputs(CABECERA, csvq);
    fprintf(ftr, "t | PID | dir virtual | pagina | offset | marco | dir fisica | resultado\n");

    printf("%-6s %-4s %6s %8s %8s %6s %7s %6s\n", "ALGO", "PLAN", "MARCOS", "AVG_WT", "AVG_TAT", "PF", "UTIL%", "CS");
    for (int pl = PLAN_RR; pl <= PLAN_SJF; pl++)
    for (int al = FIFO; al <= RELOJ; al++)
        for (int marcos = 2; marcos <= 24; marcos += 2) {
            int q = (pl == PLAN_RR) ? 3 : 0;
            bool demo = (pl == PLAN_RR && al == LRU && marcos == 4);
            Resultado r = correr(carga, n, pl, al, marcos, q, demo ? ftr : NULL, demo ? flog : NULL);
            escribir_fila(csv, pl, al, marcos, q, r);
            if (pl == PLAN_RR || al == LRU)
                printf("%-6s %-4s %6d %8.2f %8.2f %6d %7.2f %6d\n", NOMBRE_ALGO[al], NOMBRE_PLAN[pl],
                    marcos, r.avg_wt, r.avg_tat, r.total_pf, r.util_cpu, r.cambios_ctx);
        }

    /* Barrido de quantum ampliado hasta 50 ms (rango recomendado en la semana 9) */
    int quantums[] = { 1, 2, 3, 4, 5, 8, 10, 15, 20, 25, 30, 40, 50 };
    int marcos_q[] = { 6, 20 };
    int num_q = (int)(sizeof quantums / sizeof quantums[0]);
    for (int m = 0; m < 2; m++)
        for (int k = 0; k < num_q; k++) {
            Resultado r = correr(carga, n, PLAN_RR, LRU, marcos_q[m], quantums[k], NULL, NULL);
            escribir_fila(csvq, PLAN_RR, LRU, marcos_q[m], quantums[k], r);
        }

    fclose(csv); fclose(csvq); fclose(ftr); fclose(flog);
    return 0;
}