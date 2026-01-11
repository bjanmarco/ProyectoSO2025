#include <stdio.h>
#include <string.h>
#include "hardware.h"
#include "../logger.h" 

// Aqui esta la memoria principal de la maquina
Word main_memory[MEM_SIZE];
// Este semaforo es el candado para que nadie mas use el Bus
sem_t system_bus_lock;

/*
 * Inicialización de la Memoria
 * Borramos todo y creamos el candado (semáforo)
 */
void memory_init() {
    // Poner ceros en toda la memoria (memset es mas rapido que un for)
    memset(main_memory, 0, sizeof(main_memory));
    
    // Iniciamos el semaforo.
    // El '1' al final significa que empieza libre (verde).
    sem_init(&system_bus_lock, 0, 1);
    
    log_event("Memoria lista y limpia (%d espacios)", MEM_SIZE);
}

/*
 * Escribir en Memoria
 * IMPORTANTE: Hay que pedir permiso antes de escribir!
 */
void mem_write(int address, Word data) {
    // Seguridad primero: checar que la direccion exista
    if (address < 0 || address >= MEM_SIZE) {
        log_event("ERROR: Quieres escribir fuera de la memoria! (%d)", address);
        return; 
    }

    // Pedimos el bus (Wait = esperar hasta que este libre)
    sem_wait(&system_bus_lock);
    
    // Escribimos
    main_memory[address] = data;
    
    // Soltamos el bus (Post = avisar que ya terminamos)
    sem_post(&system_bus_lock);
}

/*
 * Leer de Memoria
 * Tambien hay que usar el semaforo para que no lean mientras alguien escribe
 */
Word mem_read(int address) {
    Word data = {0, 0}; // Valor vacio por si falla

    if (address < 0 || address >= MEM_SIZE) {
        log_event("ERROR: Quieres leer fuera de la memoria! (%d)", address);
        return data;
    }

    // Pedimos el bus
    sem_wait(&system_bus_lock);
    
    // Leemos
    data = main_memory[address];
    
    // Soltamos el bus
    sem_post(&system_bus_lock);
    
    return data;
}
