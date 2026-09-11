# MyShell POSIX — Proyecto CoreOS (IF0702)

Intérprete de comandos (Shell) interactivo desarrollado en Python sobre entornos POSIX (Linux/WSL2). El proyecto implementa gestión de procesos, operaciones de Entrada/Salida a nivel de kernel mediante descriptores de archivo y manejo de señales del sistema.

---

## Requisitos y Entorno de Ejecución

- **Sistema Operativo:** Linux (Ubuntu / WSL2 en Windows)
- **Lenguaje:** Python 3.8+
- **Librerías:** Módulos nativos (`os`, `sys`, `signal`, `shlex`)

---

## Arquitectura y Estructura del Código

El proyecto sigue una arquitectura modular para separar responsabilidades y facilitar el mantenimiento:

| Archivo | Descripción / Responsabilidad |
| :--- | :--- |
| `main.py` | Punto de entrada del programa. Administra el bucle principal REPL (Read-Eval-Print Loop), captura la entrada del usuario y coordina la ejecución. |
| `executor.py` | Módulo de gestión de procesos. Implementa la creación y sincronización de procesos del sistema. |
| `file_ops.py` | Operaciones de E/S a bajo nivel usando llamadas al sistema (syscalls) directas del kernel. |
| `signals_env.py` | Configuración del entorno e intercepción de señales POSIX para el control de interrupciones. |

---

## Cumplimiento de la Rúbrica de Evaluación

### 1. Gestión de Procesos (`executor.py`)
- **`os.fork()`**: Duplica el proceso actual para crear un proceso hijo independiente.
- **`os.execvp()`**: Reemplaza la imagen de memoria del proceso hijo cargando y ejecutando el binario del comando indicado por el usuario (ej. `ls`, `pwd`, `mkdir`).
- **`os.waitpid()`**: Fuerza al proceso padre (el Shell) a esperar a que el hijo finalice su ejecución antes de volver a solicitar un nuevo comando al usuario.

### 2. Operaciones de E/S a Nivel de Kernel (`file_ops.py`)
No se utilizan las funciones de alto nivel de Python (`open()`, `read()`, `write()`). Se emplean llamadas al sistema con descriptores de archivo:

- **`mywrite <archivo> "<texto>"`**: 
  - `os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)`: Abre o crea el archivo asignando un descriptor numérico.
  - `os.write(fd, text.encode())`: Escribe los bytes directamente mediante el descriptor.
  - `os.close(fd)`: Libera el descriptor en la tabla de archivos abiertos del kernel.

- **`mycat <archivo>`**:
  - `os.open(path, os.O_RDONLY)`: Obtiene el descriptor en modo lectura.
  - `os.read(fd, buffer_size)`: Lee bloques de bytes usando un búfer de memoria.
  - `os.write(1, chunk)`: Escribe los bytes leídos en la salida estándar (`stdout`, descriptor `1`).

### 3. Manejo de Señales (`signals_env.py`)
- **`signal.signal(signal.SIGINT, handler)`**: Intercepta la señal de interrupción del teclado (`Ctrl + C`).
- Evita la terminación abrupta del programa, imprimiendo una nueva línea limpia en la consola y manteniendo la estabilidad del intérprete.

---

# Prueba de gestión de procesos (fork/execvp/waitpid)
myshell:/home/user$ ls -la

# Prueba de escritura con descriptores de bajo nivel
myshell:/home/user$ mywrite demo.txt "Prueba de syscalls"

# Prueba de lectura a nivel de kernel
myshell:/home/user$ mycat demo.txt

# Captura de señal SIGINT (presionar Ctrl + C)
^C

# Salir del programa
myshell:/home/user$ exit

---

### ¿Cómo agregarlo o modificarlo en tu proyecto?

Si deseas actualizar el archivo `README.md` directamente en tu terminal de WSL:

1. Ejecuta:
   ```bash
   code README.md