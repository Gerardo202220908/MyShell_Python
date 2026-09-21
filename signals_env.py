import os
import signal
import sys

def handle_sigint(sig, frame):
    """Manejador para la señal SIGINT (Ctrl+C) """
    # Escribir usando llamada directa al descriptor de salida estándar (1)
    os.write(1, b"\n[MyShell] Presione 'exit' para salir > ")

def init_signals():
    """Captura la señal SIGINT para que el shell no muera"""
    signal.signal(signal.SIGINT, handle_sigint)

def print_prompt():
    """Obtiene el usuario y directorio actual desde el entorno"""
    user = os.environ.get("USER", "usuario")
    try:
        cwd = os.getcwd()
    except Exception:
        cwd = "?"
    
    prompt = f"{user}@myshell-py:{cwd}$ "
    sys.stdout.write(prompt)
    sys.stdout.flush()