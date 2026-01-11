#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "hardware.h"
#include "loader.h"
#include "logger.h"

// Este es el programa principal.
// Desde aqui controlamos si estamos debugeando o corriendo normal.

void print_help() {
    printf("\n--- MUNDO DE CONTROL ---\n");
    printf(" load <archivo> : Carga tu programa a memoria\n");
    printf(" run            : Corre todo de un jalon (hasta que termine o se cicle)\n");
    printf(" debug          : Corre paso a paso para ver que pasa\n");
    printf(" registers      : Chismea como estan los registros ahorita\n");
    printf(" memory <dir>   : Ve que hay en esa direccion de memoria\n");
    printf(" exit           : Vamonos\n");
    printf("----------------------------\n");
}

// Muestra bonita la info de los registros
void show_registers() {
    printf("\n[ESTADO CPU]\n");
    printf(" AC (Acumulador): [%d] %07d\n", cpu_registers.AC.sign, cpu_registers.AC.digits);
    printf(" PC (Contador)  : %05d\n", cpu_registers.PSW.pc);
    printf(" SP (Pila)      : %05d\n", cpu_registers.SP);
    printf(" PSW (Estado)   : CC=%d Modo=%d (0=Usuario, 1=Kernel) Int=%d\n", cpu_registers.PSW.condition_code, cpu_registers.PSW.operation_mode, cpu_registers.PSW.interrupt_enable);
    printf(" IR (Instrucc)  : Op=%02d Dir=%d Val=%05d\n", cpu_registers.IR.cod_op, cpu_registers.IR.direccionamiento, cpu_registers.IR.valor);
}

// El Modo Debugger: te deja dar ENTER para avanzar
void debug_loop() {
    printf("\n*** MODO DEBUG (Paso a Paso) ***\n");
    printf("Dale ENTER para avanzar, o escribe 'q' para salir.\n");
    
    cpu_running = 1; // Asegurar que la CPU esta activa
    
    char buf[10];
    while (1) {
        printf("[PC: %05d] > ", cpu_registers.PSW.pc);
        fgets(buf, sizeof(buf), stdin);
        
        if (buf[0] == 'q') break;
        
        // Ejecutamos solo UN ciclo de reloj
        cpu_cycle();
        
        // Mostramos que paso
        printf(" ... Ejecutado. Nuevo estado:\n");
        show_registers();
        
        if (!cpu_running) {
             printf("\n>>> FIN DE PROGRAMA Detectado. Saliendo de Debug. <<<\n");
             break;
        }
    }
}

// El Modo Normal: corre rapido
void run_normal() {
    printf("\n*** EJECUTANDO MODO RAPIDO ***\n");
    printf("Si se cicla, usa Ctrl+C :)\n");
    
    // Le puse un limite por si acaso hacen un loop infinito los alumnos
    int cycles = 0;
    
    // Cambiar a MODO USUARIO para que sirva la proteccion de memoria
    cpu_registers.PSW.operation_mode = MODE_USER;
    cpu_running = 1; // Reactivar CPU si estaba detenida
    
    printf("[Simulador] Cambiando a Modo USUARIO para ejecucion.\n");
    
    while (cycles < 100000 && cpu_running) { // 100k ciclos es suficiente para pruebas
        cpu_cycle();
        cycles++;
        
        // El chequeo de ceros ya no es necesario con el Sentinel
        // Pero lo dejamos por si acaso.
    }
    
    if (!cpu_running) {
        printf("\n>>> Programa finalizado correctamente (END_PROGRAM) <<<\n");
    } else {
        printf("Terminamos la ejecucion (limite de ciclos).\n");
    }
}

int main() {
    // 1. Preparamos componentes
    logger_init("virtual_machine.log");
    memory_init();
    disk_init(); // Aunque no hace mucho todavia
    cpu_reset();
    
    printf(" === MI MAQUINA VIRTUAL 2025 ===\n");
    print_help();
    
    char command[64];
    char arg[64];
    
    // Loop de la consola (Shell basica)
    while (1) {
        printf("\nMaquina> ");
        if (!fgets(command, sizeof(command), stdin)) break;
        
        command[strcspn(command, "\n")] = 0; // quitar el enter del final
        
        // Checar comandos
        if (strncmp(command, "exit", 4) == 0) {
            break;
        } 
        else if (strncmp(command, "load ", 5) == 0) {
            sscanf(command, "load %s", arg);
            load_program(arg);
        }
        else if (strcmp(command, "run") == 0) {
            run_normal();
        }
        else if (strcmp(command, "debug") == 0) {
            debug_loop();
        }
        else if (strcmp(command, "registers") == 0) {
            show_registers();
        }
        else if (strncmp(command, "memory ", 7) == 0) {
            int addr;
            sscanf(command, "memory %d", &addr);
            Word w = mem_read(addr);
            printf(" Memoria[%d] = %d (Signo: %d)\n", addr, w.digits, w.sign);
        }
        else if (strcmp(command, "help") == 0) {
            print_help();
        }
        else {
            printf("??? Ese comando no existe. Escribe 'help'.\n");
        }
    }
    
    // Limpiar antes de irnos
    logger_close();
    disk_save();
    
    return 0;
}
