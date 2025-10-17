#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include "memoria_compartida.h"
// Definición de la unión semun necesaria para semctl
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};
// Función principal
int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Uso: %s <clave> <tam_buffer> <llave_cifrado> <archivo_fuente>\n", argv[0]);
        exit(1);
    }
    // Leer argumentos
    key_t clave_mem = atoi(argv[1]);
    int tam_buffer = atoi(argv[2]);
    unsigned char clave_cifrado = (unsigned char)atoi(argv[3]);
    char *archivo_fuente = argv[4];
    // Validar tamaño de buffer
    if (tam_buffer <= 0) {
        printf("ERROR: Tamaño de buffer inválido\n");
        exit(1);
    }

    printf("Inicializando sistema...\n");

    // Crear 5 semáforos
    int id_sem = semget(clave_mem, 5, IPC_CREAT | 0666);
    if (id_sem == -1) { perror("semget"); exit(1); }

    // Inicializar semáforos
    union semun arg;
    unsigned short valores[5] = {1, (unsigned short)tam_buffer, 0, 1, 0};
    arg.array = valores;
    if (semctl(id_sem, 0, SETALL, arg) == -1) {
        perror("semctl");
        exit(1);
    }

    // Crear memoria compartida
    size_t tam_total = sizeof(control_compartido_t) + tam_buffer * sizeof(caracter_compartido_t);
    int id_mem = shmget(clave_mem, tam_total, IPC_CREAT | 0666);
    if (id_mem == -1) { perror("shmget"); exit(1); }

    control_compartido_t *control = shmat(id_mem, NULL, 0);
    if (control == (void *)-1) { perror("shmat"); exit(1); }

    // Inicializar control
    memset(control, 0, sizeof(control_compartido_t));
    control->tam_buffer = tam_buffer;
    control->clave_cifrado = clave_cifrado;
    control->sistema_activo = 1;

    // Cargar archivo
    FILE *archivo = fopen(archivo_fuente, "r");
    if (!archivo) { perror("fopen"); exit(1); }
    // Obtener tamaño del archivo
    fseek(archivo, 0, SEEK_END); // Ir al final
    control->tam_archivo = ftell(archivo); // Tamaño del archivo
    fseek(archivo, 0, SEEK_SET); // Volver al inicio

    // Validar tamaño del archivo
    if (control->tam_archivo > TAM_MAX_ARCHIVO) {
        printf("ERROR: Archivo demasiado grande\n");
        fclose(archivo);
        exit(1);
    }
    // Leer contenido del archivo
    fread(control->contenido_archivo, 1, control->tam_archivo, archivo);
    fclose(archivo); 

    // Confirmación
    printf("Sistema inicializado: %d caracteres, buffer %d slots\n", 
           control->tam_archivo, tam_buffer);
    // Desvincular memoria compartida
    shmdt(control);
    return 0;
}