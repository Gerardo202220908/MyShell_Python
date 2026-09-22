import os
import signal
import sys

def handle_sigint(sig, frame):
    """
    Manejador para la señal SIGINT (Ctrl+C).
    Evita que el Shell colapse y mantiene la ejecucion activa.
    """
    os.write(1, b"\n[MyShell] Presione 'exit' para salir > ")

def init_signals():
    """Captura la señal SIGINT a nivel de proceso"""
    signal.signal(signal.SIGINT, handle_sigint)

def print_prompt():
    """Obtiene el usuario y directorio actual respaldandose en variables de entorno"""
    user = os.environ.get("USER") or os.environ.get("LOGNAME") or "usuario"
    
    try:
        cwd = os.getcwd()
    except Exception:
        cwd = os.environ.get("PWD", "?")
    
    prompt = f"{user}@myshell-py:{cwd}$ "
    sys.stdout.write(prompt)
    sys.stdout.flush()