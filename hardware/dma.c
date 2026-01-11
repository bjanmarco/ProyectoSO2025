#include <stdio.h>
#include <unistd.h> // Para sleep y usleep
#include "hardware.h"
#include "../logger.h"

// Variables para el DMA
// Aqui guardamos el estado del controlador DMA
DMA_Controller dma;
// Esta bandera le dice a la CPU si el DMA termino su trabajo
int interrupt_pending_dma = 0; 

/*
 * Función del Hilo DMA
 * Esta funcion corre en paralelo con la CPU para simular que el disco es lento.
 * El profe dijo que usaramos hilos, asi que aqui esta.
 */
void *dma_thread_func(void *arg) {
    (void)arg; // No usamos esto, pero hay que ponerlo para que compile sin warnings
    
    log_event("[DMA] Iniciando transferencia de datos...");
    dma.is_busy = 1; // Marcamos que estamos ocupados para que no nos manden otra cosa

    // Simulamos que el disco tarda en buscar el dato (Seek Time)
    // Le ponemos 1 segundo para que se note en la ejecucion paso a paso
    sleep(1); 
    
    // Ahora si, vamos a copiar los datos.
    // Primero necesitamos pedir permiso para usar la memoria (el Bus).
    // Usamos un semaforo para que la CPU no toque la memoria mientras nosotros escribimos.
    sem_wait(&system_bus_lock);
    
    // La direccion de memoria donde vamos a leer o escribir es:
    int ram_addr = dma.memory_address;

    // Dependiendo de si es lectura o escritura:
    // dma.io_direction: 0 = Leer disco a RAM, 1 = Escribir RAM a disco
    if (dma.io_direction == 0) {
        // LEER DEL DISCO -> ESCRIBIR EN RAM
        // Simulación: Como no tenemos disco real con pistas/sectores formateados,
        // vamos a generar un dato "quemandolo" o simulado. 
        // En un caso real leeriamos de un archivo binario usando track/cylinder/sector.
        // Aqui guardaremos un valor dummy que representa el dato leido.
        Word dato_leido;
        dato_leido.sign = 0;
        // Inventamos un dato basado en el sector para saber que es distinto
        dato_leido.digits = dma.selected_sector * 1111; 
        
        // Escribimos en RAM
        if (ram_addr >= 0 && ram_addr < MEM_SIZE) {
             main_memory[ram_addr] = dato_leido;
             log_event("[DMA] Dato %d escrito en Memoria[%d]", dato_leido.digits, ram_addr);
        } else {
             log_event("[DMA] Error: Direccion de memoria invalida %d", ram_addr);
             dma.status = 1; // Error
        }
        
    } else {
        // ESCRIBIR EN DISCO <- LEER DE RAM
        if (ram_addr >= 0 && ram_addr < MEM_SIZE) {
            Word dato_a_guardar = main_memory[ram_addr];
            // Aqui "guardariamos" en el archivo de disco.
            // Solo lo logueamos por ahora.
            log_event("[DMA] Dato %d leido de Memoria[%d] y guardado en disco (simulado)", dato_a_guardar.digits, ram_addr);
        } else {
             log_event("[DMA] Error: Direccion de memoria invalida %d", ram_addr);
             dma.status = 1; // Error
        }
    }
    
    // Ya terminamos con la memoria, soltamos el bus
    sem_post(&system_bus_lock);
    
    // Finalizar
    dma.is_busy = 0; // Ya no estamos ocupados
    dma.status = 0; // Todo salio bien (0 = exito)
    
    // Avisarle al procesador que terminamos
    interrupt_pending_dma = 1;
    log_event("[DMA] Transferencia terminada. Avisando a CPU con interrupcion.");

    return NULL;
}

// Funcion para arrancar el DMA
// El profe pide que esto lance el hilo
void dma_start_transfer() {
    // Primero checamos si no esta haciendo algo ya
    if (dma.is_busy) {
        log_event("[DMA] Oye, espera! El DMA esta ocupado todavía.");
        return;
    }
    
    // Creamos el hilo del DMA
    // pthread_create(puntero_thread, atributos, funcion, argumentos)
    if (pthread_create(&dma.thread_id, NULL, dma_thread_func, NULL) != 0) {
        log_event("[DMA] No se pudo crear el hilo. Algo fallo en el sistema.");
        dma.status = 1; // Error
    }
}
