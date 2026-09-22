import os
import sys

def execute_external_command(args):
    """
    Ejecuta comandos externos creando un proceso hijo (fork), reemplazando la imagen (execvp)
    y esperando la finalización (waitpid). Retorna el código de salida real de POSIX.
    """
    try:
        pid = os.fork()
        
        if pid == 0:
            # Proceso Hijo: Reemplaza imagen de memoria
            try:
                os.execvp(args[0], args)
            except FileNotFoundError:
                print(f"myshell: comando no encontrado: {args[0]}", file=sys.stderr)
                os._exit(127)
            except Exception as e:
                print(f"myshell: error de ejecucion en '{args[0]}': {e}", file=sys.stderr)
                os._exit(126)
        else:
            # Proceso Padre: Espera activa/bloqueante
            _, status = os.waitpid(pid, 0)
            
            # Evaluar estado de salida de POSIX
            if os.WIFEXITED(status):
                return os.WEXITSTATUS(status)  # Retorna el estado real (0 = éxito)
            elif os.WIFSIGNALED(status):
                return 128 + os.WTERMSIG(status) # Proceso interrumpido por señal
                
            return 1

    except OSError as e:
        print(f"myshell: error al crear proceso (fork): {os.strerror(e.errno)}", file=sys.stderr)
        return 1