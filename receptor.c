#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>
#include <signal.h>
#include "memoria_compartida.h"

// Wrapper para operaciones de semáforo (esperar)
void sem_esperar(int id_sem, int num) {
    struct sembuf op = {num, -1, 0};
    semop(id_sem, &op, 1);
}
// Wrapper para operaciones de semáforo (señal)
void sem_signal(int id_sem, int num) {
    struct sembuf op = {num, 1, 0};
    semop(id_sem, &op, 1);
}

// Manejador de señales para ignorar Ctrl+C
void manejar_senal(int sig) {
    // Ignorar la señal
}
// Función principal
int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Uso: %s <clave_mem> <manual|automatico> <clave_cifrado>\n", argv[0]);
        exit(1);
    }

    // No permitir terminación por Ctrl+C
    signal(SIGINT, manejar_senal); // Ignorar Ctrl+C
    signal(SIGTERM, manejar_senal); // Ignorar terminación estándar
    signal(SIGQUIT, manejar_senal); // Ignorar quit
    // Parámetros
    key_t clave_mem = atoi(argv[1]);
    char *modo = argv[2];
    unsigned char clave = (unsigned char)atoi(argv[3]);

    // Conectar a recursos
    int id_mem = shmget(clave_mem, 0, 0666);
    if (id_mem == -1) { perror("shmget receptor"); exit(1); }
    // Adjuntar segmento de memoria compartida
    control_compartido_t *control = shmat(id_mem, NULL, 0);
    if (control == (void *)-1) { perror("shmat receptor"); exit(1); }
    // Apuntar al buffer circular
    caracter_compartido_t *buffer = (caracter_compartido_t *)(control + 1);
    int id_sem = semget(clave_mem, 5, 0666);
    if (id_sem == -1) { perror("semget receptor"); exit(1); }
    // Abrir archivo de salida
    FILE *salida = fopen("salida.txt", "w"); // Se escribe el resultado aquí
    if (!salida) { perror("fopen salida"); exit(1); }

    // Registrar receptor
    sem_esperar(id_sem, SEM_MUTEX); // Proteger sección crítica
    int mi_id = control->total_receptores++; // Asignar ID único
    control->receptores_activos++; // Aumentar receptores activos
    sem_signal(id_sem, SEM_MUTEX); // Liberar mutex

    printf("\033[36mReceptor %d iniciado (PID: %d) - Ctrl+C DESHABILITADO\033[0m\n", mi_id, getpid());

    // Bucle principal
    int ejecutando = 1;
    while (ejecutando) {
        // Esperar dato disponible O finalización
        struct sembuf ops[2] = {
            {SEM_LLENOS, -1, 0},
            {SEM_FINALIZACION, 0, IPC_NOWAIT}
        };
        
        if (semop(id_sem, ops, 1) == -1) {
            break;
        }

        // Leer y procesar
        sem_esperar(id_sem, SEM_MUTEX); // Proteger acceso al buffer
        int idx = control->indice_lectura; // Índice de lectura
        caracter_compartido_t dato = buffer[idx]; // Leer dato
        unsigned char decodificado = dato.valor_ascii ^ clave; //Decodificar
        // Avanzar índice de lectura
        control->indice_lectura = (idx + 1) % control->tam_buffer;
        control->contador_caracteres--;
        control->total_leido++;
        sem_signal(id_sem, SEM_MUTEX); // Liberar mutex
        // Liberar espacio en buffer
        sem_signal(id_sem, SEM_VACIOS);

        // Escribir y mostrar
        fputc(decodificado, salida);
        fflush(salida);

        // Mostrar informacion en consola
        printf("\033[34mReceptor %d:\033[0m Cifrado: %d -> Char: '%c' ASCII: %d | Índice: %d | Progreso: %d/%d\n",
               mi_id, (int)dato.valor_ascii, decodificado, (int)decodificado, idx, control->total_leido, control->tam_archivo);

        // Modo de operación
        if (strcmp(modo, "manual") == 0) {
            getchar();
        } else {
            sleep(1);
        }
    }

    fclose(salida);

    // Desregistrar
    sem_esperar(id_sem, SEM_MUTEX); // Proteger sección crítica
    control->receptores_activos--; // Disminuir receptores activos
    sem_signal(id_sem, SEM_MUTEX); // Liberar mutex

    printf("\033[33mReceptor %d finalizado elegantemente\033[0m\n", mi_id);
    shmdt(control); // Desconectar memoria compartida
    return 0;
}