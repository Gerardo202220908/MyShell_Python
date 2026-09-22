import os
import sys

def write_all(fd, data):
    """Asegura la escritura completa de los bytes requeridos en el descriptor de archivo."""
    total_written = 0
    while total_written < len(data):
        written = os.write(fd, data[total_written:])
        if written == 0:
            raise OSError("No se pudieron escribir mas bytes en el descriptor.")
        total_written += written

def builtin_cat(args):
    """Comando interno 'mycat <archivo>' con manejo de errores y cierre garantizado."""
    if len(args) < 2:
        print("Uso: mycat <nombre_archivo>", file=sys.stderr)
        return 1

    fd = -1
    try:
        fd = os.open(args[1], os.O_RDONLY)
        
        while True:
            chunk = os.read(fd, 1024)
            if not chunk:
                break
            os.write(1, chunk)
            
        return 0
        
    except PermissionError:
        print(f"myshell: permiso denegado para leer '{args[1]}'", file=sys.stderr)
        return 1
    except OSError as e:
        print(f"myshell: error al abrir/leer '{args[1]}': {os.strerror(e.errno)}", file=sys.stderr)
        return 1
    finally:
        if fd != -1:
            os.close(fd) # Cierre garantizado del recurso

def builtin_write(args):
    """Comando interno 'mywrite <archivo> <texto>' usando write_all y try/finally."""
    if len(args) < 3:
        print('Uso: mywrite <nombre_archivo> "<texto>"', file=sys.stderr)
        return 1

    file_path = args[1]
    text_content = " ".join(args[2:]) + "\n"
    fd = -1

    try:
        flags = os.O_CREAT | os.O_WRONLY | os.O_TRUNC
        fd = os.open(file_path, flags, 0o644)
        
        encoded_data = text_content.encode('utf-8')
        write_all(fd, encoded_data)
        
        print(f"Archivo '{file_path}' escrito exitosamente mediante llamadas al kernel.")
        return 0
        
    except OSError as e:
        print(f"myshell: error al escribir en '{file_path}': {os.strerror(e.errno)}", file=sys.stderr)
        return 1
    finally:
        if fd != -1:
            os.close(fd) # Cierre garantizado del recurso

def builtin_env(args):
    """Comando interno 'myenv' o 'getenv <var>' para validar variables de entorno."""
    if len(args) == 1:
        # Imprimir todas las variables de entorno
        for key, value in os.environ.items():
            print(f"{key}={value}")
        return 0
    elif len(args) >= 2:
        # Obtener variable específica
        var_name = args[1]
        val = os.environ.get(var_name)
        if val is not None:
            print(val)
            return 0
        else:
            print(f"myshell: variable '{var_name}' no definida", file=sys.stderr)
            return 1
    return 1