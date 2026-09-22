#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

extern char **environ;

/* Remover comillas alrededor de argumentos */
void clean_quotes(char *str) {
    if (str == NULL) return;
    int len = strlen(str);
    if (len >= 2 && ((str[0] == '"' && str[len - 1] == '"') || (str[0] == '\'' && str[len - 1] == '\''))) {
        memmove(str, str + 1, len - 2);
        str[len - 2] = '\0';
    }
}

/* Manejador de señal SIGINT (Ctrl+C) */
void handle_sigint(int sig) {
    write(1, "\n[MyShell-C] Presione 'exit' para salir > ", 41);
}

/* Comando interno mycat */
void builtin_cat(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "myshell: uso: mycat <nombre_archivo>\n");
        return;
    }
    
    clean_quotes(args[1]);
    
    int fd = open(args[1], O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "myshell: error al abrir '%s': %s\n", args[1], strerror(errno));
        return;
    }
    
    char buffer[1024];
    ssize_t bytes_read;
    
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        write(1, buffer, bytes_read);
    }
    
    if (bytes_read < 0) {
        fprintf(stderr, "myshell: error al leer archivo: %s\n", strerror(errno));
    }
    
    close(fd);
}

/* Comando interno mywrite con garantia de escritura de bajo nivel */
void builtin_write(char **args) {
    if (args[1] == NULL || args[2] == NULL) {
        fprintf(stderr, "myshell: uso: mywrite <nombre_archivo> \"<texto>\"\n");
        return;
    }
    
    clean_quotes(args[1]);
    
    int fd = open(args[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        fprintf(stderr, "myshell: error al abrir/crear archivo: %s\n", strerror(errno));
        return;
    }
    
    for (int i = 2; args[i] != NULL; i++) {
        char temp[1024];
        strcpy(temp, args[i]);
        clean_quotes(temp);
        write(fd, temp, strlen(temp));
        if (args[i+1] != NULL) write(fd, " ", 1);
    }
    write(fd, "\n", 1);
    
    close(fd);
    printf("Archivo '%s' escrito exitosamente mediante llamadas al kernel.\n", args[1]);
}

/* Comando interno para variables de entorno (myenv / getenv) */
void builtin_env(char **args) {
    if (args[1] == NULL) {
        for (char **env = environ; *env != NULL; env++) {
            printf("%s\n", *env);
        }
    } else {
        clean_quotes(args[1]);
        char *val = getenv(args[1]);
        if (val) {
            printf("%s\n", val);
        } else {
            fprintf(stderr, "myshell: variable '%s' no definida\n", args[1]);
        }
    }
}

/* Ejecucion de procesos externos (fork, execvp, waitpid) */
void execute_external_command(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        clean_quotes(args[i]);
    }

    pid_t pid = fork();
    
    if (pid < 0) {
        fprintf(stderr, "myshell: error al ejecutar fork(): %s\n", strerror(errno));
        return;
    } else if (pid == 0) {
        execvp(args[0], args);
        fprintf(stderr, "myshell: comando no encontrado: %s: %s\n", args[0], strerror(errno));
        _exit(127);
    } else {
        int status;
        while (waitpid(pid, &status, 0) < 0) {
            if (errno == ECHILD) break;
        }
    }
}

int main() {
    signal(SIGINT, handle_sigint);
    
    char line[MAX_LINE];
    char *args[MAX_ARGS];
    
    char *user = getenv("USER");
    if (!user) user = getenv("LOGNAME");
    if (!user) user = "usuario";

    while (1) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            strcpy(cwd, "?");
        }

        printf("%s@myshell-c:%s$ ", user, cwd);
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\nSaliendo de MyShell...\n");
            break;
        }

        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;

        int argc = 0;
        char *token = strtok(line, " ");
        while (token != NULL && argc < MAX_ARGS - 1) {
            args[argc++] = token;
            token = strtok(NULL, " ");
        }
        args[argc] = NULL;

        if (args[0] == NULL) continue;

        if (strcmp(args[0], "exit") == 0) {
            printf("Saliendo de MyShell...\n");
            break;
        } else if (strcmp(args[0], "mycat") == 0) {
            builtin_cat(args);
        } else if (strcmp(args[0], "mywrite") == 0) {
            builtin_write(args);
        } else if (strcmp(args[0], "myenv") == 0 || strcmp(args[0], "getenv") == 0) {
            builtin_env(args);
        } else {
            execute_external_command(args);
        }
    }

    return 0;
}