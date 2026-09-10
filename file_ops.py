import os
import sys

def builtin_cat(args):
    """Comando interno 'mycat <archivo>' usando llamadas directas al kernel"""
    if len(args) < 2:
        print("Uso: mycat <nombre_archivo>", file=sys.stderr)
        return 1

    try:
        # 1. open() directo al SO para obtener el descriptor de archivo (fd)
        fd = os.open(args[1], os.O_RDONLY)
        
        # 2. read() y write() directos
        while True:
            # Lee un bloque de 1024 bytes desde el descriptor de archivo
            chunk = os.read(fd, 1024)
            if not chunk:
                break
            # Escribe directamente en el descriptor 1 (stdout - salida estándar)
            os.write(1, chunk)
            
        # 3. close() para liberar el descriptor
        os.close(fd)
        
    except FileNotFoundError:
        print(f"myshell: el archivo '{args[1]}' no existe", file=sys.stderr)
    except Exception as e:
        print(f"myshell: error al leer archivo: {e}", file=sys.stderr)

    return 1

def builtin_write(args):
    """Comando interno 'mywrite <archivo> <texto>' directo al kernel"""
    if len(args) < 3:
        print('Uso: mywrite <nombre_archivo> "<texto>"', file=sys.stderr)
        return 1

    file_path = args[1]
    text_content = " ".join(args[2:]) + "\n"

    try:
        # Abre/Crea el archivo con banderas POSIX y permisos 0o644 (rw-r--r--)
        flags = os.O_CREAT | os.O_WRONLY | os.O_TRUNC
        fd = os.open(file_path, flags, 0o644)
        
        # Convierte el string a bytes para la llamada os.write()
        encoded_data = text_content.encode('utf-8')
        os.write(fd, encoded_data)
        
        # Cierra el descriptor
        os.close(fd)
        print(f"Archivo '{file_path}' escrito exitosamente mediante llamadas al kernel.")
        
    except Exception as e:
        print(f"myshell: error al escribir en archivo: {e}", file=sys.stderr)

    return 1