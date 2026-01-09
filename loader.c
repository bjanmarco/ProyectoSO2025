#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "hardware.h"
#include "loader.h"
#include "logger.h"

int load_program(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("Error: No se pudo abrir el archivo %s\n", filename);
        return -1;
    }

    char line[256];
    int start_address = 0;
    int instructions_loaded = 0;
    
    log_event("Iniciando carga de programa: %s", filename);

    while (fgets(line, sizeof(line), f)) {
        // Remover salto de linea
        line[strcspn(line, "\n")] = 0;
        
        // Ignorar lineas vacias
        if (strlen(line) == 0) continue;

        // Metadatos
        if (strncmp(line, "_start", 6) == 0) {
            sscanf(line, "_start %d", &start_address);
            // Validar que start_address esté en memoria USUARIO
            if (start_address < USER_MEM_START) {
                printf("Error: Direccion de inicio invalida (Area de SO reservada)\n");
                log_event("Error carga: _start %d invalido", start_address);
                fclose(f);
                return -1;
            }
            cpu_registers.PSW.pc = start_address;
            log_event("Punto de entrada definido: %d", start_address);
        }
        else if (strncmp(line, ".NumeroPalabras", 15) == 0) {
            // Informativo o para validación
            int count;
            sscanf(line, ".NumeroPalabras %d", &count);
            log_event("Metadata: Palabras esperadas = %d", count);
        }
        else if (strncmp(line, ".NombreProg", 11) == 0) {
            log_event("Metadata: Nombre Programa = %s", line + 12);
        }
        else if (line[0] == '.') {
            // Fin de bloque o archivo
            continue;
        }
        else if (line[0] == '/') {
            // Comentario
            continue;
        }
        else {
            // Se asume que es una instrucción numérica
            // Formato esperado: 12345678 (8 digitos)
            // Leeremos como entero
            int instruction_val;
            // Asegurarnos que es numérico
            if (sscanf(line, "%d", &instruction_val) == 1) {
                if (start_address + instructions_loaded >= MEM_SIZE) {
                    printf("Error: Programa excede memoria disponible\n");
                    break;
                }
                
                // Convertir int a Word y guardar en memoria
                Word w = int_to_word(instruction_val);
                mem_write(start_address + instructions_loaded, w);
                
                // Log debug (verborrágico)
                // log_event("Cargado [%d]: %08d", start_address + instructions_loaded, instruction_val);
                
                instructions_loaded++;
            }
        }
    }

    fclose(f);
    printf("Programa cargado exitosamente. %d instrucciones.\n", instructions_loaded);
    log_event("Carga finalizada. %d instrucciones en memoria.", instructions_loaded);
    
    // Configurar Registros Base y Limite para el proceso cargado
    // Simplificación: Asignamos todo el espacio de usuario restante
    cpu_registers.RB = USER_MEM_START;
    cpu_registers.RL = MEM_SIZE - 1; // Hasta el final
    // Pila al final de la memoria asignada
    cpu_registers.SP = cpu_registers.RL;
    cpu_registers.RX = cpu_registers.RL; // Base de pila (aprox)
    
    // Cambiar a MODO USUARIO para ejecutar (según spec, arrancamos en consola, luego user mode al correr)
    // Pero el reset pone Kernel. El comando RUN cambiará a User.
    
    return 0;
}
