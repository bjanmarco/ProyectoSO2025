#include <stdio.h>
#include <string.h>
#include "hardware.h"
#include "../logger.h"

// Variable Global definida en hardware.h
HardDisk hdd;

// Nombre del archivo de persistencia
#define DISK_FILENAME "virtual_disk.bin"

/*
 * Inicialización del Disco
 * Intenta cargar el archivo "virtual_disk.bin". Si no existe, lo crea vacío.
 */
void disk_init() {
    FILE *f = fopen(DISK_FILENAME, "rb");
    if (f) {
        // Cargar disco existente
        fread(&hdd, sizeof(HardDisk), 1, f);
        fclose(f);
        log_event("Disco cargado desde %s", DISK_FILENAME);
    } else {
        // Crear disco nuevo (vacío)
        memset(&hdd, 0, sizeof(HardDisk));
        
        // Guardar para crear el archivo
        f = fopen(DISK_FILENAME, "wb");
        if (f) {
            fwrite(&hdd, sizeof(HardDisk), 1, f);
            fclose(f);
        }
        log_event("Disco nuevo inicializado y guardado en %s", DISK_FILENAME);
    }
}

/*
 * Persistir Disco
 * Guarda el estado actual de la estructura HardDisk en el archivo.
 * Esto simula que los datos quedan grabados magnéticamente.
 */
void disk_save() {
    FILE *f = fopen(DISK_FILENAME, "wb");
    if (f) {
        fwrite(&hdd, sizeof(HardDisk), 1, f);
        fclose(f);
        // log_event("Estado del disco guardado."); // Demasiado ruido si se llama mucho
    } else {
        log_event("ERROR: No se pudo guardar el disco en %s", DISK_FILENAME);
    }
}
