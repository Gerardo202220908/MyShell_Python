import os
import sys

def execute_external_command(args):
    """Manejo de llamadas al sistema fork, execvp y waitpid [Tanenbaum 1.6.1]"""
    try:
        # 1. Duplicar el proceso actual mediante fork()
        pid = os.fork()
        
        if pid < 0:
            print("myshell: Error al ejecutar os.fork()", file=sys.stderr)
            return 1
            
        elif pid == 0:
            # 2. Proceso Hijo: Reemplaza su imagen de memoria usando execvp()
            # execvp busca automáticamente el ejecutable en el $PATH del sistema
            try:
                os.execvp(args[0], args)
            except FileNotFoundError:
                print(f"myshell: comando no encontrado: {args[0]}", file=sys.stderr)
                os._exit(1)
            except Exception as e:
                print(f"myshell: error de ejecucion: {e}", file=sys.stderr)
                os._exit(1)
                
        else:
            # 3. Proceso Padre: Espera la finalización del hijo con waitpid()
            while True:
                _, status = os.waitpid(pid, 0)
                if os.WIFEXITED(status) or os.WIFSIGNALED(status):
                    break
                    
    except Exception as e:
        print(f"myshell: Error al gestionar el proceso: {e}", file=sys.stderr)
        
    return 1