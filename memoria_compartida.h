#ifndef MEMORIA_COMPARTIDA_H
#define MEMORIA_COMPARTIDA_H

#include <sys/types.h>
#include <time.h>

#define TAM_MAX_ARCHIVO 5000000
// Estructura para representar un caracter compartido
typedef struct {
    unsigned char valor_ascii;
    int indice;
    time_t marca_tiempo;
    pid_t pid_emisor;
} caracter_compartido_t;
// Estructura para el control compartido    
typedef struct {
    // Configuración
    int tam_buffer;
    unsigned char clave_cifrado;
    // Archivo
    char contenido_archivo[TAM_MAX_ARCHIVO];
    int tam_archivo;
    // Buffer circular
    int indice_escritura;
    int indice_lectura;
    int contador_caracteres;
    // Estadísticas
    int sistema_activo;
    int total_caracteres_transferidos;
    int total_emisores;
    int total_receptores;
    int emisores_activos;
    int receptores_activos;
    int total_leido;
    int total_escrito;
    
} control_compartido_t;
// Definiciones de semáforos
#define SEM_MUTEX 0 // Semáforo de exclusión mutua
#define SEM_VACIOS 1  // Semáforo de espacios vacíos
#define SEM_LLENOS 2   // Semáforo de elementos llenos
#define SEM_ARCHIVO 3   // Semáforo para acceso al archivo
#define SEM_FINALIZACION 4  // Semáforo para finalización ordenada

#endif