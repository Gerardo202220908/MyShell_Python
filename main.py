import sys
import shlex
from signals_env import init_signals, print_prompt
from executor import execute_external_command
from file_ops import builtin_cat, builtin_write

def main():
    # Registrar captura de señales (SIGINT - Ctrl+C) [Tanenbaum 1.8]
    init_signals()

    while True:
        try:
            print_prompt()
            line = sys.stdin.readline()
            
            # EOF (Ctrl+D) finaliza el shell
            if not line:
                print("\nSaliendo de MyShell...")
                break
                
            line = line.strip()
            if not line:
                continue

            # Tokenizar argumentos respectando comillas
            args = shlex.split(line)
            
            # Salida explícita
            if args[0] == "exit":
                print("Saliendo de MyShell...")
                break
                
            # Comandos built-in de archivos directos al kernel
            elif args[0] == "mycat":
                builtin_cat(args)
            elif args[0] == "mywrite":
                builtin_write(args)
            else:
                # Ejecutar comando mediante fork(), execvp() y waitpid()
                execute_external_command(args)

        except KeyboardInterrupt:
            # Manejo secundario por si interrumpe el input de sys.stdin
            continue
        except Exception as e:
            print(f"Error inesperado: {e}", file=sys.stderr)

if __name__ == "__main__":
    main()