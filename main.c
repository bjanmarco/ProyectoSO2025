#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "hardware.h"
#include "loader.h"
#include "logger.h"

// Variable para controlar el hilo de CPU si decidimos correrlo aparte
// En esta implementación simple (monohilo para el step-by-step), 
// controlaremos cpu_cycle() desde el main loop en modo debug,
// o un loop continuo en modo normal.

void print_help() {
    printf("\n--- Comandos Disponibles ---\n");
    printf(" load <archivo> : Carga un programa en memoria\n");
    printf(" run            : Ejecuta el programa en Modo Normal (hasta finalizar)\n");
    printf(" debug          : Entra en Modo Debugger (paso a paso)\n");
    printf(" registers      : Muestra el estado de los registros\n");
    printf(" memory <dir>   : Muestra el contenido de una direccion de memoria\n");
    printf(" exit           : Salir del simulador\n");
    printf("----------------------------\n");
}

void show_registers() {
    printf("\n[Registros CPU]\n");
    printf(" AC:  [%d] %07d\n", cpu_registers.AC.sign, cpu_registers.AC.digits);
    printf(" PC:  %05d\n", cpu_registers.PSW.pc);
    printf(" SP:  %05d\n", cpu_registers.SP);
    printf(" PSW: CC=%d Mode=%d Int=%d\n", cpu_registers.PSW.condition_code, cpu_registers.PSW.operation_mode, cpu_registers.PSW.interrupt_enable);
    printf(" IR:  Op=%02d Mode=%d Val=%05d\n", cpu_registers.IR.cod_op, cpu_registers.IR.direccionamiento, cpu_registers.IR.valor);
}

void debug_loop() {
    printf("\n*** MODO DEBUGGER ***\n");
    printf("Presione ENTER para siguiente instruccion, 'q' para salir al menu principal.\n");
    
    char buf[10];
    while (1) {
        printf("[PC: %05d] > ", cpu_registers.PSW.pc);
        fgets(buf, sizeof(buf), stdin);
        
        if (buf[0] == 'q') break;
        
        // Ejecutar un ciclo
        cpu_cycle();
        
        // Mostrar Estado
        Word last_ir = mem_read(cpu_registers.PSW.pc - 1); // Pq PC ya incrementó
        // Recalcular para mostrar lo que SE EJECUTÓ
        // Esto es tricky, mejor mostrar IR actual (que ya tiene la instr)
        
        printf(" Ejecutado: Op %d | Resultado AC: %d\n", cpu_registers.IR.cod_op, cpu_registers.AC.digits);
        show_registers();
        
        // Chequear errores/halt
        // Como sabemos si terminó? (Spec no tiene HALT, asumimos loop o RETRN final?)
        // Spec no define instruccion HALT. Asumiremos que si PC apunta a 0 o algo invalido se detiene,
        // o el usuario decide cuando parar.
    }
}

void run_normal() {
    printf("\n*** EJECUTANDO MODO NORMAL ***\n");
    printf("Presione Ctrl+C para interrumpir (o espere finalizacion si hay logica de fin).\n");
    
    // Limite de seguridad para evitar loops infinitos en pruebas
    int cycles = 0;
    while (cycles < 100000) { // 100k ciclos tope
        cpu_cycle();
        cycles++;
        
        // Aqui deberiamos chequear interrupciones o condiciones de parada
        // Como no hay HALT explícito, es difícil saber cuándo parar automático.
        // Quizas si PC llega a una direccion vacía (0)?
        Word w = mem_read(cpu_registers.PSW.pc);
        if (w.digits == 0 && w.sign == 0) {
            // Asumimos NOP/Fin si hay ceros consecutivos?
            // Riesgo: codigo valido puede ser 0.
            // Mejor no parar automático salvo error.
        }
    }
    printf("Ejecucion normal pausada (limite ciclos o fin).\n");
}

int main() {
    // Inicialización
    logger_init("virtual_machine.log");
    memory_init();
    disk_init();
    cpu_reset();
    
    // Inicializar DMA (sin start, solo struct)
    // El hilo DMA start se hace con instrucción.
    
    printf(" === SIMULADOR HARDWARE VIRTUAL ===\n");
    print_help();
    
    char command[64];
    char arg[64];
    
    while (1) {
        printf("\nShell> ");
        if (!fgets(command, sizeof(command), stdin)) break;
        
        command[strcspn(command, "\n")] = 0; // strip newline
        
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
            printf(" Mem[%d] = %d (Sign: %d)\n", addr, w.digits, w.sign);
        }
        else if (strcmp(command, "help") == 0) {
            print_help();
        }
        else {
            printf("Comando desconocido.\n");
        }
    }
    
    // Cleanup
    logger_close();
    disk_save();
    
    return 0;
}
