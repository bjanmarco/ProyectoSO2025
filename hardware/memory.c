#include <stdio.h>
#include <string.h>
#include "hardware.h"
#include "../logger.h" // Asumiendo que logger.h está en el root, ajustaremos paths en Makefile

// Variables Globales definidas en hardware.h
Word main_memory[MEM_SIZE];
sem_t system_bus_lock;

/*
 * Inicialización de la Memoria
 */
void memory_init() {
    // Limpiar memoria
    memset(main_memory, 0, sizeof(main_memory));
    
    // Inicializar semáforo del bus (valor 1 = desbloqueado)
    sem_init(&system_bus_lock, 0, 1);
    
    log_event("Memoria inicializada (%d palabras)", MEM_SIZE);
}

/*
 * Escritura en Memoria (Con protección de bus)
 */
void mem_write(int address, Word data) {
    if (address < 0 || address >= MEM_SIZE) {
        log_event("ERROR FATAL: Intento de escritura fuera de rango: %d", address);
        return; 
        // En una implementación real, esto debería lanzar una excepción hardware
        // pero aquí lo manejaremos en la CPU antes de llamar a esto usualmente.
    }

    // Adquirir el bus
    sem_wait(&system_bus_lock);
    
    main_memory[address] = data;
    
    // Liberar el bus
    sem_post(&system_bus_lock);
}

/*
 * Lectura de Memoria (Con protección de bus)
 */
Word mem_read(int address) {
    Word data = {0, 0};

    if (address < 0 || address >= MEM_SIZE) {
        log_event("ERROR FATAL: Intento de lectura fuera de rango: %d", address);
        return data;
    }

    // Adquirir el bus
    sem_wait(&system_bus_lock);
    
    data = main_memory[address];
    
    // Liberar el bus
    sem_post(&system_bus_lock);
    
    return data;
}
