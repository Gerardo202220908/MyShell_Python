import sys
import shlex
from signals_env import init_signals, print_prompt
from executor import execute_external_command
from file_ops import builtin_cat, builtin_write, builtin_env

def main():
    init_signals()

    while True:
        try:
            print_prompt()
            line = sys.stdin.readline()

            if not line:  # Manejo de EOF (Ctrl+D)
                print("\nSaliendo de MyShell...")
                break

            line = line.strip()
            if not line:
                continue

            args = shlex.split(line)
            if not args:
                continue

            command = args[0]

            if command == "exit":
                print("Saliendo de MyShell...")
                break
            elif command == "mycat":
                builtin_cat(args)
            elif command == "mywrite":
                builtin_write(args)
            elif command in ["myenv", "getenv"]:
                builtin_env(args)
            else:
                execute_external_command(args)

        except ValueError as e:
            print(f"myshell: error de sintaxis en el comando: {e}", file=sys.stderr)
        except Exception as e:
            print(f"myshell: error inesperado: {e}", file=sys.stderr)

if __name__ == "__main__":
    main()