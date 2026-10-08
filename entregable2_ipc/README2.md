# Entregable 2: Productor-Consumidor -- Buffer Acotado POSIX

Módulo de Concurrencia e IPC para **CoreOS (IF0702)** - Universidad Ricardo Palma (2026-II).

## 📌 Descripción del Proyecto
Este proyecto implementa la solución clásica al problema del **Productor-Consumidor** utilizando hilos POSIX (`pthread`) y semáforos en C sobre un entorno Linux (WSL2 / Ubuntu). Garantiza la exclusión mutua, sincronización por eventos, prevención estricta de interbloqueos (*deadlocks*) mediante el ordenamiento de recursos de Coffman y auditoría de invariantes en tiempo de ejecución.

---

##  Requisitos del Sistema
* **OS:** Linux / WSL2
* **Compilador:** GCC (`gcc`)
* **Librerías:** `libpthread`
* **Herramientas de Auditoría:** `valgrind` (módulo `helgrind`)

---

##  Compilación y Ejecución

### 1. Compilación
Compila el proyecto activando todas las advertencias (`-Wall`) y enlazando la biblioteca de hilos (`-pthread`):

```bash
gcc -Wall -pthread productor_consumidor.c -o productor_consumidor

* **Modo Normal:** Ritmo equilibrado con retardo aleatorio estocástico (50–250 ms) mediante `rand_r()`.

```bash
./productor_consumidor normal

Aquí tienes el texto completo en un solo bloque de código para que lo copies directamente a tu `README.md`:

```markdown
* **Modo Normal:** Ritmo equilibrado con retardo aleatorio estocástico (50–250 ms) mediante `rand_r()`.

```bash
./productor_consumidor normal

```

* **Modo Productor Rápido:** Productores sin retardo. Satura el buffer a capacidad máxima (`5/5`) y demuestra el bloqueo pasivo en `sem_wait(&vacios)`.

```bash
./productor_consumidor prod-rapido

```

* **Modo Consumidor Rápido:** Consumidores sin retardo. Vacía el buffer a `0/5` y demuestra el bloqueo pasivo en `sem_wait(&llenos)`.

```bash
./productor_consumidor cons-rapido

```

---

## Pruebas de Estrés y Auditoría Concurrente

### Prueba de Estrés (20 ejecuciones en ráfaga)

```bash
for i in {1..20}; do ./productor_consumidor > /dev/null || break; done && echo "20/20 ejecuciones exitosas sin Deadlock"

```

### Auditoría con Helgrind (Valgrind)

Verificación de ausencia de condiciones de carrera (*data races*):

```bash
valgrind --tool=helgrind ./productor_consumidor

```

*(Resultado esperado: `ERROR SUMMARY: 0 errors`)*

```

```
