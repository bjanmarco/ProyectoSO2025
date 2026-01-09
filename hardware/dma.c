#include <stdio.h>
#include <unistd.h> // Para sleep y usleep
#include "hardware.h"
#include "../logger.h"

// Definición de variable global DMA
DMA_Controller dma;
int interrupt_pending_dma = 0; // Definición del flag de interrupción

/*
 * Función del Hilo DMA
 * Simula la operación asíncrona de E/S
 */
void *dma_thread_func(void *arg) {
    (void)arg; // Unused
    
    log_event("[DMA] Iniciando operación de transferencia...");
    dma.is_busy = 1;

    // Simular tiempo de búsqueda (seek time) + transferencia
    // 1 segundo de "simulación" para que sea perceptible
    sleep(1); 
    
    // Realizar la transferencia real
    // dma.io_direction: 0 = Leer de disco a RAM, 1 = Escribir de RAM a disco
    
    // Calcular "dirección física" en disco lineal
    // Esto es una simplificación, copiamos el sector entero
    // int disk_idx = ...
    
    // Adquirir bus para escribir/leer RAM
    sem_wait(&system_bus_lock);
    
    // Aquí implementaremos la lógica de copia usando dma.memory_address y el buffer del disco
    // ...
    
    sem_post(&system_bus_lock);
    
    // Finalizar
    dma.is_busy = 0;
    dma.status = 0; // Éxito (por ahora siempre 0)
    
    // Generar Interrupción
    interrupt_pending_dma = 1;
    log_event("[DMA] Operación finalizada. Interrupción generada.");
    log_interrupt(INT_IO_DONE, "DMA Transferencia Completa");

    return NULL;
}

void dma_start_transfer() {
    if (dma.is_busy) {
        log_event("[DMA] ERROR: Intento de iniciar transferencia con DMA ocupado.");
        return;
    }
    
    // Crear el hilo para ejecutar la transferencia
    if (pthread_create(&dma.thread_id, NULL, dma_thread_func, NULL) != 0) {
        log_event("[DMA] ERROR CRITICO: No se pudo crear hilo DMA.");
        dma.status = 1; // Error
    }
}
