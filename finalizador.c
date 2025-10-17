#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <signal.h>
#include "memoria_compartida.h"

// wrappers para operaciones de semáforo
void sem_signal(int id_sem, int num) {
    struct sembuf op = {num, 1, 0};
    semop(id_sem, &op, 1);
}
// Funcion principal
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <clave_mem>\n", argv[0]);
        exit(1);
    }
    // Obtener clave de memoria compartida
    key_t clave_mem = atoi(argv[1]);

    printf("\033[35m FINALIZADOR DEL SISTEMA \033[0m\n");
    printf("Este es el ÚNICO proceso que puede terminar el sistema\n");
    printf("Presionar Enter para iniciar apagado controlado...\n");
    getchar();

    // Conectar a recursos
    int id_mem = shmget(clave_mem, 0, 0666);
    if (id_mem == -1) { 
        printf("Error: No se encontró memoria compartida. No se ejecuto inicializador\n");
        return 1; 
    }
    // Adjuntar memoria compartida
    control_compartido_t *control = shmat(id_mem, NULL, 0);
    if (control == (void *)-1) { perror("shmat finalizador"); return 1; }
    // Obtener semáforos
    int id_sem = semget(clave_mem, 5, 0666);
    if (id_sem == -1) { 
        printf("Error: No se encontraron semáforos. No se ejecuto inicializador\n");
        shmdt(control);
        return 1; 
    }

    printf("Iniciando apagado controlado...\n");

    // Señalar finalización a todos los procesos
    printf("Enviando señal de finalización a todos los procesos...\n");
    for (int i = 0; i < control->tam_buffer * 2; i++) {
        sem_signal(id_sem, SEM_FINALIZACION);
    }

    // Desbloquear procesos que puedan estar esperando
    printf("Desbloqueando procesos...\n");
    for (int i = 0; i < control->tam_buffer * 2; i++) {
        sem_signal(id_sem, SEM_VACIOS);
        sem_signal(id_sem, SEM_LLENOS);
    }

    printf("Esperando que procesos terminen correctamente...\n");

    // Esperar pasivamente a que todos terminen
    int esperando = 1;
    while (esperando) {
        sleep(1);
        // Verificar estado actual
        struct sembuf op = {SEM_MUTEX, -1, IPC_NOWAIT};
        if (semop(id_sem, &op, 1) == 0) {
            int procesos_activos = control->emisores_activos + control->receptores_activos;
            sem_signal(id_sem, SEM_MUTEX);
            
            if (procesos_activos == 0) {
                esperando = 0;
                printf("Todos los procesos han terminado elegantemente\n");
            } else {
                printf("[%02ds] Procesos activos: %d emisores, %d receptores\n",control->emisores_activos, control->receptores_activos);
            }
        }
    }
    // Mostrar estadísticas finales
    printf("\n\033[35m=== ESTADÍSTICAS FINALES ===\033[0m\n");
    printf("Caracteres transferidos: %d\n", control->total_caracteres_transferidos);
    printf("Caracteres en buffer: %d/%d\n", control->contador_caracteres, control->tam_buffer);
    printf("Emisores: %d totales, %d activos\n", control->total_emisores, control->emisores_activos);
    printf("Receptores: %d totales, %d activos\n", control->total_receptores, control->receptores_activos);
    printf("Progreso: %d/%d caracteres procesados\n", control->total_leido, control->tam_archivo);
    if (control->tam_archivo > 0) {
        float porcentaje = (control->total_leido * 100.0) / control->tam_archivo; //Calcular el porcentaje de completado
        printf("Completado: %.1f%%\n", porcentaje);
    }

    // Limpiar recursos
    printf("\nLimpiando recursos del sistema...\n");
    shmdt(control);
    // Eliminar memoria compartida y semáforos
    if (shmctl(id_mem, IPC_RMID, NULL) == 0) {
        printf("Memoria compartida eliminada\n");
    }
    // Eliminar semáforos
    if (semctl(id_sem, 0, IPC_RMID) == 0) {
        printf("Semáforos eliminados\n");
    }

    printf("\033[32mSistema finalizado correctamente\033[0m\n");
    return 0;
}