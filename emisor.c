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

//wrapper para operaciones de semáforo (esperar)
void sem_esperar(int id_sem, int num) {
    struct sembuf op = {num, -1, 0};
    semop(id_sem, &op, 1);
}
//wrapper para operaciones de semáforo (señal)
void sem_signal(int id_sem, int num) {
    struct sembuf op = {num, 1, 0};
    semop(id_sem, &op, 1);
}

// Manejador de señales para ignorar Ctrl+C
void manejar_senal(int sig) {
    // ignorar la señal 
}

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
    int id_mem = shmget(clave_mem, 0, 0666); // Obtenemos el ID del segmento existente
    if (id_mem == -1) { perror("shmget emisor"); exit(1); } 
    
    control_compartido_t *control = shmat(id_mem, NULL, 0); // Adjuntamos el segmento
    if (control == (void *)-1) { perror("shmat emisor"); exit(1); }

    caracter_compartido_t *buffer = (caracter_compartido_t *)(control + 1); // Apuntamos al buffer circular
    int id_sem = semget(clave_mem, 5, 0666); // Obtenemos el ID del conjunto de semáforos
    if (id_sem == -1) { perror("semget emisor"); exit(1); }

    // Registrar emisor
    sem_esperar(id_sem, SEM_MUTEX); 
    int mi_id = control->total_emisores++; // Asignar ID único
    control->emisores_activos++;   // Aumentar los emisores activos
    sem_signal(id_sem, SEM_MUTEX); // Liberar mutex

    printf("\033[32mEmisor %d iniciado (PID: %d) - Ctrl+C DESHABILITADO\033[0m\n", mi_id, getpid());

    // Bucle principal
    int ejecutando = 1;
    while (ejecutando) {
        // Slot vacio o finalización
        struct sembuf ops[2] = {
            {SEM_VACIOS, -1, 0},
            {SEM_FINALIZACION, 0, IPC_NOWAIT}
        };
        
        if (semop(id_sem, ops, 1) == -1) {
            break;
        }

        // Obtener próximo carácter
        sem_esperar(id_sem, SEM_ARCHIVO); // Proteger acceso a archivo
        if (control->total_escrito >= control->tam_archivo) { 
            sem_signal(id_sem, SEM_ARCHIVO); // Liberar semáforo de archivo
            sem_signal(id_sem, SEM_VACIOS); // Devolver el espacio reservado
            break;
        }
        // Leer carácter y avanzar
        unsigned char c = control->contenido_archivo[control->total_escrito];
        control->total_escrito++; 
        sem_signal(id_sem, SEM_ARCHIVO); 

        // Procesar y escribir en buffer
        unsigned char cifrado = c ^ clave;
        
        // Escribir en buffer circular
        sem_esperar(id_sem, SEM_MUTEX);
        int indice = control->indice_escritura;
        buffer[indice] = (caracter_compartido_t){cifrado, indice, time(NULL), getpid()};
        control->indice_escritura = (indice + 1) % control->tam_buffer;
        control->contador_caracteres++;
        control->total_caracteres_transferidos++;
        sem_signal(id_sem, SEM_MUTEX);

        sem_signal(id_sem, SEM_LLENOS);

        // Mostrar informacion en consola
        printf("\033[32mEmisor %d:\033[0m Char: '%c' ASCII: %d -> Cifrado: %d | Índice: %d | Progreso: %d/%d\n",
               mi_id, c, (int)c, (int)cifrado, indice, control->total_escrito, control->tam_archivo);

        // Modo de operación
        if (strcmp(modo, "manual") == 0) {
            getchar();
        } else {
            sleep(1);
        }
    }

    // Desregistrar
    sem_esperar(id_sem, SEM_MUTEX); // Proteger sección crítica
    control->emisores_activos--; // Disminuir emisores activos
    sem_signal(id_sem, SEM_MUTEX); // Liberar mutex

    printf("\033[33mEmisor %d finalizado elegantemente\033[0m\n", mi_id);
    shmdt(control);
    return 0;
}