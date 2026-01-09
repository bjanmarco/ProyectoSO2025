#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "hardware.h"
#include "../logger.h"

Registers cpu_registers;

/* =========================================================================
 * FUNCIONES AUXILIARES
 * ========================================================================= */

// Convierte un Word (Signo, Magnitud) a int (C estándar)
int word_to_int(Word w) {
    int val = w.digits;
    if (w.sign == 1) val = -val;
    return val;
}

// Convierte int (C estándar) a Word (Signo, Magnitud)
Word int_to_word(int val) {
    Word w;
    if (val < 0) {
        w.sign = 1;
        w.digits = -val;
    } else {
        w.sign = 0;
        w.digits = val;
    }
    // Truncar a 7 dígitos de magnitud por seguridad -> REMOVED to allow 8-digit instructions
    // w.digits %= 10000000;
    return w;
}

/*
 * Reinicia los registros de la CPU a un estado conocido
 */
void cpu_reset() {
    // cpu_registers.PC = 0; // Eliminado, usar PSW.pc
    
    cpu_registers.SP = 0; // Pila vacía (tope inicial)
    cpu_registers.RX = 0;
    cpu_registers.RB = 0;
    cpu_registers.RL = MEM_SIZE - 1; // Por defecto todo es accesible hasta que el SO diga lo contrario
    
    // PSW Inicial: Modo Kernel, Interrupciones Deshabilitadas
    cpu_registers.PSW.operation_mode = MODE_KERNEL;
    cpu_registers.PSW.interrupt_enable = INT_DISABLED;
    cpu_registers.PSW.condition_code = CC_ZERO;
    cpu_registers.PSW.pc = 0;
    
    // Limpiar AC, IR, MAR, MDR
    cpu_registers.AC.sign = 0; cpu_registers.AC.digits = 0;
    cpu_registers.IR.cod_op = 0; cpu_registers.IR.direccionamiento = 0; cpu_registers.IR.valor = 0;
    
    log_event("CPU Reseteada. PC=0, Modo=KERNEL");
}

// Actualiza el Condition Code (CC) basado en el AC actual
void update_cc() {
    int val = word_to_int(cpu_registers.AC);
    if (val == 0) cpu_registers.PSW.condition_code = CC_ZERO;
    else if (val < 0) cpu_registers.PSW.condition_code = CC_NEGATIVE;
    else cpu_registers.PSW.condition_code = CC_POSITIVE;
    // Overflow se maneja por separado en aritmetica
}

// Verifica si una dirección es válida dado el modo (Usuario vs Kernel)
// Si hay error, genera interrupción y retorna 0. Retorna 1 si OK.
int check_memory_protection(int address) {
    // En Modo Kernel, acceso total
    if (cpu_registers.PSW.operation_mode == MODE_KERNEL) return 1;

    // En Modo Usuario, verificar RB <= address <= RL
    // RECORDAR: address aquí es DIRECTA (física) si ya se sumó el offset, 
    // PERO la especificación dice: "Dirección Relativa + RB". 
    // En este simulador, la dirección que llega aquí ya debería ser la final.
    // Asumiremos que 'address' es la dirección efectiva.
    
    // NOTA: La especificación dice "Toda dirección es relativa al proceso".
    // Esto implica que las direcciones lógicas 0..N se mapean a RB..RB+N.
    // Sin embargo, las instrucciones 'Direccionamiento Directo' dan una dirección final.
    // Para simplificar, asumiremos que si es Modo Usuario, la instrucción ya trae la dirección LÓGICA,
    // y nosotros debemos sumar RB y chequear RL.
    
    // correccion: las instrucciones usan direccionamiento. Vamos a asumir que las funciones de resolución
    // de direcciones hacen la traducción y chequeo.
    // Si la dirección 'address' es la *absoluta* (después de sumar RB), chqueamos limities.
    
    if (address < cpu_registers.RB || address > cpu_registers.RL) {
        log_interrupt(INT_ADDR_INVALID, "Violacion de segmento (Direccion Fuera de Rango)");
        return 0;
    }
    return 1;
}

// Maneja la generación de interrupciones
void generate_interrupt(int code) {
    // Salvaguarda de contexto (Simplificado: Pone PC en Stack o similar?)
    // La especificación dice "La máquina debe salvaguardar registros".
    // Implementación simple: Push PC & PSW to Stack System o similar.
    // Por simplicidad educativa: Solo saltamos al vector y logueamos.
    // Un OS real salvaría todo. Aqui asumiremos que el handler del OS (sw) lo hace
    // o que es mágico.
    
    log_instruction(cpu_registers.PSW.pc, "INT_HANDLER", code);
    
    // Buscar dirección del manejador en vector (simulado)
    // Asumimos vector en direcciones bajas (ej: 0..10) memoria OS
    // O simplificamos: El PC salta a una dirección fija de manejo de interrupciones
    // Digamos dirección 10 + code
    
    // OJO: La especificación no dice dónde está el vector.
    // Asumiremos que el OS (loader) pone "jumps" en las direcciones 0-9.
    
    // Cambiar a Modo Kernel
    // int old_mode = cpu_registers.PSW.operation_mode;
    cpu_registers.PSW.operation_mode = MODE_KERNEL;
    cpu_registers.PSW.interrupt_enable = INT_DISABLED; // Deshabilitar int anidadas
    
    // Salvar PC viejo en Pila (simulado, o el OS deberia hacerlo?) 
    // Haremos PUSH del PC actual para poder volver con RETRN?
    // La instrucción RETRN dice "restaura PC desde pila". 
    // Así que SÍ, debemos empujar el PC antes de saltar.
    
    cpu_registers.SP--;
    mem_write(cpu_registers.SP, int_to_word(cpu_registers.PSW.pc));
    
    // Saltar al manejador (Direccion = code * 10, por ejemplo)
    // O simplemente asumimos que el vector esta en 'code'.
    cpu_registers.PSW.pc = code * 10; // Ejemplo: Int 2 -> Dir 20
}

/* 
 * Resolución de Operandos según Direccionamiento
 * Devuelve el VALOR del operando (si es inmediato) o el DATOS de memoria (si es directo/indexado)
 * Si devuelve -1 indica error (excepción generada dentro).
 * PERO espera, necesitamos diferenciar Valor vs Dirección.
 * Instrucciones LOAD necesitan el valor. STR necesitan la dirección.
 */
 
int get_effective_address() {
    int addr = -1;
    int mode = cpu_registers.IR.direccionamiento;
    int val = cpu_registers.IR.valor;
    
    if (mode == ADDR_DIRECT) {
        addr = val;
    } else if (mode == ADDR_INDEXED) {
        // Indexado: Dirección = Base (AC o RX?) + Desplazamiento
        // Spec: "5 últimos dígitos son un índice a partir del acumulador"
        // o "RX + valor"? La diapositiva dice "indice a partir del acumulador".
        // Usualmente es Base + Index. Asumiremos Address = AC + Valor (del IR).
        addr = word_to_int(cpu_registers.AC) + val;
    } else if (mode == ADDR_IMMEDIATE) {
        // No tiene dirección efectiva
        return -1;
    }
    
    // Ajuste de Relocalización y Protección (Base Limit)
    if (cpu_registers.PSW.operation_mode == MODE_USER) {
        // Si modo usuario, dirección es relativa a RB
        addr += cpu_registers.RB;
        
        // Protección
        if (addr > cpu_registers.RL) { // RB ya está implícito al sumar
            log_interrupt(INT_ADDR_INVALID, "Violacion de segmento");
            return -2; // Error
        }
    }
    
    return addr;
}

/* =========================================================================
 * IMPLEMENTACIÓN DE INSTRUCCIONES
 * ========================================================================= */

void exec_arithmetic(int opcode) {
    int operand_val = 0;
    
    if (cpu_registers.IR.direccionamiento == ADDR_IMMEDIATE) {
        operand_val = cpu_registers.IR.valor;
    } else {
        int addr = get_effective_address();
        if (addr < 0) return; // Error ya manejado
        operand_val = word_to_int(mem_read(addr));
    }
    
    int ac_val = word_to_int(cpu_registers.AC);
    long long res = 0; // Usar long long para detectar overflow

    switch(opcode) {
        case OP_SUM: 
            res = (long long)ac_val + operand_val; 
            break;
        case OP_RES: 
            res = (long long)ac_val - operand_val; 
            break;
        case OP_MULT: 
            res = (long long)ac_val * operand_val; 
            break;
        case OP_DIVI: 
            if (operand_val == 0) {
                log_interrupt(INT_INST_INVALID, "Division por Cero"); 
                // Usando cod 5 pq no hay especifico, o 8? Mejor 5 'Instruccion invalida' o custom.
                return;
            }
            res = ac_val / operand_val; 
            break;
    }
    
    // Check Overflow (limite 7 digitos = 9999999)
    if (res > 9999999 || res < -9999999) {
        cpu_registers.PSW.condition_code = CC_OVERFLOW;
        log_interrupt(INT_OVERFLOW, "Desbordamiento Aritmetico");
        // Truncar para seguir
        res = res % 10000000;
    }
    
    cpu_registers.AC = int_to_word((int)res);
    update_cc();
}

void exec_transfer_mem(int opcode) {
    int addr = get_effective_address();
    if (addr < 0 && opcode != OP_LOAD) return; // Error de dir
    // OP_LOAD admite Inmediato? Spec dice: "Carga en AC el contenido de la posición... (según modo)"
    // Si modo=Inmediato, LOAD carga el valor LITERAL.
    
    if (opcode == OP_LOAD) {
        if (cpu_registers.IR.direccionamiento == ADDR_IMMEDIATE) {
            cpu_registers.AC = int_to_word(cpu_registers.IR.valor);
        } else {
            if (addr < 0) return;
            cpu_registers.AC = mem_read(addr);
        }
    } else if (opcode == OP_STR) {
        if (cpu_registers.IR.direccionamiento == ADDR_IMMEDIATE) {
            // STR inmediato no tiene sentido (store 5 in 10?) -> Error
            log_interrupt(INT_INST_INVALID, "STR Inmediato no valido");
            return;
        }
        if (addr < 0) return;
        mem_write(addr, cpu_registers.AC);
    }
}

void exec_jump(int opcode) {
    int addr = get_effective_address();
    // Saltos suelen ser a direcciones directas de codigo.
    // get_effective_address ya maneja la relocalización si es usuario.
    if (addr < 0) return;
    
    int jump = 0;
    int sp_val_int = 0;
    
    // Para saltos condicionales, comparamos AC con M[SP] (Tope Pila)
    // OJO: Spec dice "Si AC == M[SP]".
    if (opcode != OP_J) {
        Word sp_val = mem_read(cpu_registers.SP);
        sp_val_int = word_to_int(sp_val);
    }
    
    int ac_val = word_to_int(cpu_registers.AC);

    switch(opcode) {
        case OP_J: jump = 1; break;
        case OP_JMPE:  if (ac_val == sp_val_int) jump = 1; break;
        case OP_JMPNE: if (ac_val != sp_val_int) jump = 1; break;
        case OP_JMPLT: if (ac_val < sp_val_int)  jump = 1; break;
        case OP_JMPLGT:if (ac_val > sp_val_int)  jump = 1; break;
    }
    
    if (jump) {
        cpu_registers.PSW.pc = addr;
    }
}

void exec_comp() {
    int val = 0;
    if (cpu_registers.IR.direccionamiento == ADDR_IMMEDIATE) {
        val = cpu_registers.IR.valor;
    } else {
        int addr = get_effective_address();
        if (addr < 0) return;
        val = word_to_int(mem_read(addr));
    }
    
    int ac_val = word_to_int(cpu_registers.AC);
    
    if (ac_val == val) cpu_registers.PSW.condition_code = CC_ZERO;
    else if (ac_val < val) cpu_registers.PSW.condition_code = CC_NEGATIVE;
    else cpu_registers.PSW.condition_code = CC_POSITIVE;
}

void exec_stack(int opcode) {
    if (opcode == OP_PSH) {
        cpu_registers.SP--;
        // Validar Overflow de Pila (si SP baja de su limite)
        // Ojo con colisiones Heap/Stack
        mem_write(cpu_registers.SP, cpu_registers.AC);
    } else if (opcode == OP_POP) {
        cpu_registers.AC = mem_read(cpu_registers.SP);
        cpu_registers.SP++;
    }
}

/* =========================================================================
 * LOOP PRINCIPAL
 * ========================================================================= */

void cpu_cycle() {
    // 0. Chequear Interrupciones Hardware (DMA)
    if (interrupt_pending_dma && cpu_registers.PSW.interrupt_enable) {
        interrupt_pending_dma = 0; // Clear line
        generate_interrupt(INT_IO_DONE);
        return; // Atender inmediatamente
    }

    // 1. FETCH
    int pc = cpu_registers.PSW.pc;
    // Validar PC dentro de limites
    // (Si estamos en usuario, PC es relativo? Asumiremos PC absoluto por simplicidad en simulador,
    // pero manejado con offset al inicio del programa).
    // Para simplificar, PC es absoluto.
    
    if (pc >= MEM_SIZE) {
        log_event("FATAL: PC fuera de memoria (%d)", pc);
        return;
    }

    Word instruction_word = mem_read(pc);
    
    // Log
    log_instruction(pc, "FETCH", instruction_word.digits);

    // Increment PC
    cpu_registers.PSW.pc++;

    // 2. DECODE
    int raw = instruction_word.digits;
    cpu_registers.IR.valor = raw % 100000;
    cpu_registers.IR.direccionamiento = (raw / 100000) % 10;
    cpu_registers.IR.cod_op = (raw / 1000000);
    
    int op = cpu_registers.IR.cod_op;

    // 3. EXECUTE
    switch(op) {
        // Grupo 1: Aritmética
        case OP_SUM: case OP_RES: case OP_MULT: case OP_DIVI:
            exec_arithmetic(op); 
            break;
            
        // Grupo 2: Transferencia Memoria
        case OP_LOAD: case OP_STR:
            exec_transfer_mem(op); 
            break;
            
        // Grupo 3: Transferencia Registros Especiales
        case OP_LOADRX: cpu_registers.AC = int_to_word(cpu_registers.RX); break;
        case OP_STRRX:  cpu_registers.RX = word_to_int(cpu_registers.AC); break;
        
        // Grupo 4: Comparación y Saltos Condicionales
        case OP_COMP:   exec_comp(); break;
        case OP_JMPE: case OP_JMPNE: case OP_JMPLT: case OP_JMPLGT:
            exec_jump(op); 
            break;
            
        // Grupo 8: Salto Incondicional (Puesto aqui por similitud)
        case OP_J:
            exec_jump(op); 
            break;
            
        // Grupo 5: Sistema
        case OP_SVC:
            generate_interrupt(INT_SVC);
            break;
        case OP_RETRN:
             // Pop PC from Stack
             cpu_registers.PSW.pc = word_to_int(mem_read(cpu_registers.SP));
             cpu_registers.SP++;
             // Restaurar modo anterior? La spec no detalla, asumimos retorno simple.
             break;
        case OP_HAB:  cpu_registers.PSW.interrupt_enable = INT_ENABLED; break;
        case OP_DHAB: cpu_registers.PSW.interrupt_enable = INT_DISABLED; break;
        case OP_TTI:
             // Timer simulado.
             break;
        case OP_CHMOD:
             if (cpu_registers.PSW.operation_mode == MODE_KERNEL) {
                 cpu_registers.PSW.operation_mode = (cpu_registers.PSW.operation_mode == MODE_KERNEL) ? MODE_USER : MODE_KERNEL;
             }
             break;
             
        // Grupo 6: Registros Base/Limite
        case OP_LOADRB: cpu_registers.AC = int_to_word(cpu_registers.RB); break;
        case OP_STRRB:  cpu_registers.RB = word_to_int(cpu_registers.AC); break;
        case OP_LOADRL: cpu_registers.AC = int_to_word(cpu_registers.RL); break;
        case OP_STRRL:  cpu_registers.RL = word_to_int(cpu_registers.AC); break;
        case OP_LOADSP: cpu_registers.AC = int_to_word(cpu_registers.SP); break;
        case OP_STRSP:  cpu_registers.SP = word_to_int(cpu_registers.AC); break;
        
        // Grupo 7: Pila
        case OP_PSH: case OP_POP:
            exec_stack(op);
            break;
            
        // Grupo 9: DMA
        case OP_SDMAP: dma.selected_track = cpu_registers.IR.valor; break;
        case OP_SDMAC: dma.selected_cylinder = cpu_registers.IR.valor; break;
        case OP_SDMAS: dma.selected_sector = cpu_registers.IR.valor; break;
        case OP_SDMAIO:dma.io_direction = cpu_registers.IR.valor; break;
        case OP_SDMAM: dma.memory_address = cpu_registers.IR.valor; break;
        case OP_SDMAON: dma_start_transfer(); break;
            
        default:
            log_interrupt(INT_INST_INVALID, "Opcode Invalido");
            break;
    }
}
