# MyShell POSIX — Proyecto CoreOS (IF0702)

**MyShell POSIX** es un intérprete de comandos (*shell*) interactivo desarrollado bajo una arquitectura modular desacoplada en **Python 3** y respaldado por una versión binaria equivalente en **C nativo (`myshell.c`)**. El proyecto demuestra de manera práctica el cruce de fronteras entre el Espacio de Usuario (*User Space*) y el Espacio del Kernel (*Kernel Space*) mediante llamadas al sistema (*system calls*) directas en entornos POSIX (Linux / WSL2).

---

## Integrantes del Grupo
* Matias Guevara, Gerardo Joel
* Ochoa Zuñiga, Marcelo Fabian
* Barcena Aldave, Giovanni Franco Luca
* Canchos Lopez, Alex Josemir
* Caballero Medina, Gianfranco

---

## Requisitos del Entorno de Ejecución

- **Sistema Operativo:** Linux (Ubuntu nativo o WSL2 en Windows).
- **Entorno Python:** Python 3.8+ (Utiliza exclusivamente la biblioteca estándar: `os`, `sys`, `signal`, `shlex`, `unittest`).
- **Entorno C (Opcional para binario):** Compilador `gcc` (`build-essential`).

---

## Arquitectura y Estructura del Código

El proyecto sigue una arquitectura modular en Python para garantizar la separación de responsabilidades:

| Archivo / Módulo | Responsabilidad y Mapeo POSIX |
| :--- | :--- |
| `main.py` | Punto de entrada del programa. Administra el bucle **REPL** (*Read-Eval-Print Loop*), captura entradas y realiza tokenización segura respetando cadenas entre comillas mediante `shlex.split()`. |
| `executor.py` | Gestor de procesos externos. Implementa la creación, reemplazo de memoria y sincronización mediante `os.fork()`, `os.execvp()` y `os.waitpid()`. |
| `file_ops.py` | Operaciones de E/S a bajo nivel y variables de entorno. Omite librerías de alto nivel utilizando `os.open()`, `os.read()`, `os.write()`, `os.close()` y `os.environ`. |
| `signals_env.py` | Control de interrupciones asíncronas y prompt dinámico. Registra el manejador de la señal `SIGINT` (`Ctrl + C`) mediante `signal.signal()`. |
| `myshell.c` | Respaldo en C nativo compilable para evaluación estricta a nivel binario POSIX. |
| `test_suite.py` | Suite de pruebas unitarias automatizadas basadas en `unittest`. |

---

## Funcionalidades e Implementación Técnica

### 1. Gestión de Procesos (`executor.py`)
- **`os.fork()`**: Duplica el proceso padre para crear un proceso hijo independiente en el espacio de usuario.
- **`os.execvp()`**: Reemplaza la imagen de memoria del proceso hijo cargando el binario del comando solicitado (ej. `ls`, `pwd`). Si falla, retorna código POSIX `127`.
- **`os.waitpid()`**: Bloquea al proceso padre hasta que el hijo finalice, recuperando el código de estado real (*exit status*) mediante `WIFEXITED` y `WEXITSTATUS`.

### 2. Operaciones de Entrada/Salida a Nivel de Kernel (`file_ops.py`)
No utiliza abstracciones de alto nivel de Python (`open()`, `read()`, `write()` de archivos standard). Opera sobre descriptores de archivo numéricos:

- **`mywrite <archivo> "<texto>"`**:
  - Abre o crea el archivo con banderas POSIX (`O_CREAT | O_WRONLY | O_TRUNC`) y permisos `0644`.
  - Garantiza la escritura completa del buffer mediante la función auxiliar `write_all()`.
  - Asegura la liberación del descriptor en el Kernel mediante bloques `try/finally`.
- **`mycat <archivo>`**:
  - Abre el archivo en modo de solo lectura (`O_RDONLY`).
  - Lee bloques fijos de `1024 bytes` usando `os.read()`.
  - Vuelca los datos directamente a la salida estándar (`stdout`, descriptor `1`) con `os.write(1, chunk)`.

### 3. Variables de Entorno y Control de Señales (`file_ops.py` / `signals_env.py`)
- **`getenv <VARIABLE>` / `myenv <VARIABLE>`**: Consulta el valor de una variable específica en `os.environ`.
- **`myenv` (sin argumentos)**: Muestra el mapa completo de variables de entorno exportadas en la sesión.
- **Captura de `SIGINT` (`Ctrl + C`)**: Manejador `handle_sigint()` intercepta la interrupción del teclado mediante `os.write()`, evitando el colapso del Shell y redibujando el prompt interactivo.

---

## Guía de Ejecución y Pruebas

### Opción A: Ejecución en Python

1. **Iniciar el Shell interactivo:**
   ```bash
   python3 main.py

   # Comandos externos (fork + execvp + waitpid)
ls -la
pwd

# Operaciones I/O a bajo nivel
mywrite demo.txt "Prueba de kernel POSIX"
mycat demo.txt

# Variables de entorno
getenv USER
myenv

# Intercepción de teclado (Presionar Ctrl + C en el teclado)
^C

# Salir de la sesión
exit

# Opción B: Ejecución de la Suite Automatizada de Pruebas
Para ejecutar las 4 pruebas unitarias automatizadas integradas en la raíz del proyecto:

python3 test_suite.py

# RESULTADO ESPERADO
....
----------------------------------------------------------------------
Ran 4 tests in 0.280s

OK

# Opción C: Compilación y Ejecución del Respaldo en C Nativo
gcc myshell.c -o myshell

./myshell
