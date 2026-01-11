#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "hardware.h"
#include "../logger.h"

// Aqui estan todos los registros de mi CPU
Registers cpu_registers;

// Forward Declaration
void generate_interrupt(int code);

/* =========================================================================
 * FUNCIONES DE AYUDA
 * Son para convertir de mi estructura Word a int de C para poder sumar/restar
 * ========================================================================= */

// Convierte un Word (mi estructura) a un int normal de C
int word_to_int(Word w) {
    int val = w.digits;
    // Si el signo es 1, entonces es negativo
    if (w.sign == 1) val = -val;
    return val;
}

// Convierte un int normal a mi estructura Word
Word int_to_word(int val) {
    Word w;
    if (val < 0) {
        w.sign = 1;       // Es negativo
        w.digits = -val;  // Guardamos solo el valor positivo (magnitud)
    } else {
        w.sign = 0;       // Es positivo
        w.digits = val;
    }
    // No truncamos aqui, confiamos en que cabe o se manejo el overflow antes
    return w;
}

/*
 * Esta funcion pone todo en cero para empezar desde el principio
 * Es como reiniciar la maquina.
 */
void cpu_reset() {
    cpu_registers.SP = 0; // La pila empieza en 0? O deberia ser al final?
                          // Por ahora 0, luego el loader dice donde ponerla.
    cpu_registers.RX = 0;
    cpu_registers.RB = 0;
    cpu_registers.RL = MEM_SIZE - 1; // Al principio dejamos acceso a todo
    
    // PSW Inicial: 
    // Arrancamos en Modo Kernel (1) para poder cargar cosas.
    // Interrupciones apagadas (0) para que no nos molesten al inicio.
    cpu_registers.PSW.operation_mode = MODE_KERNEL;
    cpu_registers.PSW.interrupt_enable = INT_DISABLED; // 0
    cpu_registers.PSW.condition_code = CC_ZERO;
    cpu_registers.PSW.pc = 0; // Empezamos en la direccion 0
    
    // Limpiamos los registros de trabajo
    cpu_registers.AC.sign = 0; cpu_registers.AC.digits = 0;
    cpu_registers.IR.cod_op = 0; cpu_registers.IR.direccionamiento = 0; cpu_registers.IR.valor = 0;
    
    // Inicializar Vector de Interrupciones (0-8)
    // Apuntar a una rutina de "Panico" o "Default" en caso de que no haya SO
    // Digamos la direccion 200 para pruebas
    for (int i=0; i<=8; i++) {
        mem_write(i, int_to_word(200)); 
    }
    // Escribimos un RETRN en la direccion 200, para que si salta ahi, solo regrese.
    // Opcode RETRN = 14 -> 14000000
    mem_write(200, int_to_word(14000000));
    
    cpu_running = 1; // Encendemos motores
    log_event("CPU Reiniciada. Tabla de Vectores (0-8) apunta a 200. RUNNING=1");
}

// Esta funcion actualiza los codigos CC del PSW segun como quedo el Acumulador
void update_cc() {
    int val = word_to_int(cpu_registers.AC);
    if (val == 0) cpu_registers.PSW.condition_code = CC_ZERO;          // 0
    else if (val < 0) cpu_registers.PSW.condition_code = CC_NEGATIVE;  // 1
    else cpu_registers.PSW.condition_code = CC_POSITIVE;               // 2
    // Nota: El overflow (3) se pone directo en la operacion si pasa
}

// Verifica si tenemos permiso de entrar a esa memoria
// Esto es para proteger la memoria de otros procesos
// Verifica si tenemos permiso de entrar a esa memoria
// Esto es para proteger la memoria de otros procesos
int check_memory_protection(int address) {
    // Si soy Kernel (Superusuario), puedo hacer lo que quiera
    if (cpu_registers.PSW.operation_mode == MODE_KERNEL) return 1;

    // Si soy Usuario normal, tengo que respetar mis limites (RB y RL)
    // RB = donde empieza mi memoria
    // RL = hasta donde llega
    
    if (address < cpu_registers.RB || address > cpu_registers.RL) {
        log_interrupt(INT_ADDR_INVALID, "ERROR: Violacion de Segmento (Address fuera de RB-RL)!");
        generate_interrupt(INT_ADDR_INVALID);
        return 0; // Fallo
    }
    return 1; // Todo bien
}

// Aqui manejamos las interrupciones
// Es cuando pasa algo importante y hay que parar lo que haciamos
void generate_interrupt(int code) {
    log_instruction(cpu_registers.PSW.pc, "INTERRUPCION", code);
    
    // Validar codigo de interrupcion (0-8)
    if (code < 0 || code > 8) {
        // Evitar recursion infinita si el mismo INT_CODE_INVALID falla
        if (code != INT_CODE_INVALID) {
             generate_interrupt(INT_CODE_INVALID);
        }
        return;
    }

    // Cambiamos a Modo Kernel para atender el problema
    int old_mode = cpu_registers.PSW.operation_mode;
    int old_cc = cpu_registers.PSW.condition_code;
    int old_int = cpu_registers.PSW.interrupt_enable;
    
    cpu_registers.PSW.operation_mode = MODE_KERNEL;
    cpu_registers.PSW.interrupt_enable = INT_DISABLED; // Apagamos interrupciones anidadas
    
    // IMPORTANTE: Hay que guardar TODO EL CONTEXTO para poder volver.
    // Registros a guardar: PC, PSW, AC, RX.
    
    // 1. Push PC
    cpu_registers.SP--; 
    mem_write(cpu_registers.SP, int_to_word(cpu_registers.PSW.pc));
    
    // 2. Push Flags (Empaquetamos en un numero: 100*CC + 10*Mode + Int)
    int flags_packed = (old_cc * 100) + (old_mode * 10) + old_int;
    cpu_registers.SP--;
    mem_write(cpu_registers.SP, int_to_word(flags_packed));
    
    // 3. Push AC
    cpu_registers.SP--;
    mem_write(cpu_registers.SP, cpu_registers.AC);
    
    // 4. Push RX
    cpu_registers.SP--;
    mem_write(cpu_registers.SP, int_to_word(cpu_registers.RX));
    
    // Buscamos la direccion del manejador en la Tabla de Vectores (Memoria[code])
    Word handler_word = mem_read(code);
    int handler_addr = word_to_int(handler_word);
    
    log_event("Saltando a Manejador en %d (Leido de Memoria[%d])", handler_addr, code);
    cpu_registers.PSW.pc = handler_addr; 
}

/* 
 * Esta funcion calcula cual es la direccion real que queremos usar
 * Revisa si es Directo o Indexado.
 */
int get_effective_address() {
    int addr = -1; // -1 significa error o invalido
    int mode = cpu_registers.IR.direccionamiento;
    int val = cpu_registers.IR.valor;
    
    if (mode == ADDR_DIRECT) {
        // Directo: El valor ES la direccion
        addr = val;
    } else if (mode == ADDR_INDEXED) {
        // Indexado: La direccion es el valor + lo que haya en el Acumulador
        // Esto sirve para recorrer arreglos
        addr = word_to_int(cpu_registers.AC) + val;
    } else if (mode == ADDR_IMMEDIATE) {
        // Inmediato: No hay direccion, el valor es el dato mismo
        return -1;
    }
    
    // Ahora revisamos proteccion y relocalizacion si somos Usuario
    if (cpu_registers.PSW.operation_mode == MODE_USER) {
        // Sumamos el Registro Base
        addr += cpu_registers.RB;
        
        // Usamos la funcion centralizada de proteccion
        if (!check_memory_protection(addr)) {
            // El log ya se hizo adentro de check_memory_protection
            return -2; // Codigo de error especial
        }
    }
    
    return addr;
}

/* =========================================================================
 * AQUI SE EJECUTAN LAS INSTRUCCIONES
 * ========================================================================= */

// Ejecuta sumas, restas, multiplica, divide
void exec_arithmetic(int opcode) {
    int operand_val = 0;
    
    // Vemos si el dato viene directo en la instruccion o esta en memoria
    if (cpu_registers.IR.direccionamiento == ADDR_IMMEDIATE) {
        operand_val = cpu_registers.IR.valor; // El dato es este numero
    } else {
        int addr = get_effective_address();
        if (addr < 0) return; // Si fallo la direccion, abortamos
        operand_val = word_to_int(mem_read(addr)); // Vamos a buscarlo a memoria
    }
    
    int ac_val = word_to_int(cpu_registers.AC);
    long long res = 0; // Uso long long para que quepa si se pasa (overflow)

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
            if (operand_val == 0) { // Cuidado con dividir por cero
                log_interrupt(INT_INST_INVALID, "Error Matemático: Division por Cero"); 
                generate_interrupt(INT_INST_INVALID);
                return;
            }
            res = ac_val / operand_val; 
            break;
    }
    
    // Revisamos si el numero es muy grande para caber (Overflow)
    // Nuestro sistema aguanta hasta 7 digitos de magnitud (9,999,999)
    if (res > 9999999 || res < -9999999) {
        cpu_registers.PSW.condition_code = CC_OVERFLOW;
        log_interrupt(INT_OVERFLOW, "Desbordamiento (Numero muy grande)");
        generate_interrupt(INT_OVERFLOW);
        // Lo cortamos para que quepa, aunque este mal
        res = res % 10000000;
    }
    
    // Guardamos el resultado en el Acumulador
    cpu_registers.AC = int_to_word((int)res);
    update_cc(); // Actualizamos si es positivo, negativo o cero
}

// Mueve cosas entre Memoria y CPU
void exec_transfer_mem(int opcode) {
    int addr = get_effective_address();
    // LOAD con inmediato es especial, no necesita direccion
    if (addr < 0 && !(opcode == OP_LOAD && cpu_registers.IR.direccionamiento == ADDR_IMMEDIATE)) return; 
    
    if (opcode == OP_LOAD) {
        if (cpu_registers.IR.direccionamiento == ADDR_IMMEDIATE) {
            // LOAD Inmediato: AC = Numero
            cpu_registers.AC = int_to_word(cpu_registers.IR.valor);
        } else {
            // LOAD Memoria: AC = Memoria[addr]
            if (addr < 0) return;
            cpu_registers.AC = mem_read(addr);
        }
    } else if (opcode == OP_STR) {
        // STR (Store): Guardar AC en Memoria
        if (cpu_registers.IR.direccionamiento == ADDR_IMMEDIATE) {
            log_interrupt(INT_INST_INVALID, "No puedes hacer STR Inmediato (donde guardo?)");
            return;
        }
        if (addr < 0) return;
        mem_write(addr, cpu_registers.AC);
    }
}

// Saltos (JUMP)
void exec_jump(int opcode) {
    int addr = get_effective_address();
    if (addr < 0) return; // Direccion mala
    
    int jump = 0; // Bandera para saber si saltamos o no
    int sp_val_int = 0; // Valor del tope de la pila
    
    // Para los saltos condicionales, comparamos AC con lo que hay en el tope de la Pila
    if (opcode != OP_J) {
        Word sp_val = mem_read(cpu_registers.SP);
        sp_val_int = word_to_int(sp_val);
    }
    
    int ac_val = word_to_int(cpu_registers.AC);

    switch(opcode) {
        case OP_J: jump = 1; break; // Salto siempre
        case OP_JMPE:  if (ac_val == sp_val_int) jump = 1; break; // Salto si Igual
        case OP_JMPNE: if (ac_val != sp_val_int) jump = 1; break; // Salto si Diferente
        case OP_JMPLT: if (ac_val < sp_val_int)  jump = 1; break; // Salto si Menor
        case OP_JMPLGT:if (ac_val > sp_val_int)  jump = 1; break; // Salto si Mayor
    }
    
    if (jump) {
        // Cambiamos el PC para saltar a esa instruccion
        cpu_registers.PSW.pc = addr;
    }
}

// Comparacion
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
    
    // Solo actualizamos el PSW, no cambiamos ningun registro de datos
    if (ac_val == val) cpu_registers.PSW.condition_code = CC_ZERO;
    else if (ac_val < val) cpu_registers.PSW.condition_code = CC_NEGATIVE;
    else cpu_registers.PSW.condition_code = CC_POSITIVE;
}

// Operaciones de Pila (Stack)
void exec_stack(int opcode) {
    if (opcode == OP_PSH) {
        // Push: Meter a la pila
        cpu_registers.SP--; // La pila crece hacia abajo (direcciones menores)
        
        // CHECK OVERFLOW DE PILA (Si se cruza con Kernel o OS?)
        // Por ahora solo verificamos que no sea negativo (aunque SP es int)
        // Pero mas importante es el UNDERFLOW en POP
        mem_write(cpu_registers.SP, cpu_registers.AC); // Guardamos AC
    } else if (opcode == OP_POP) {
        // Pop: Sacar de la pila
        // CHECK UNDERFLOW
        // Si SP >= RX (Base de la Pila), significa que esta vacia (porque crece hacia abajo)
        if (cpu_registers.SP >= cpu_registers.RX) {
            log_interrupt(INT_UNDERFLOW, "Error: Stack Underflow (Pila Vacia)");
            generate_interrupt(INT_UNDERFLOW);
            return;
        }
        
        cpu_registers.AC = mem_read(cpu_registers.SP); // Recuperamos a AC
        cpu_registers.SP++; // "Borramos" subiendo el puntero
    }
}

/* =========================================================================
 * CICLO PRINCIPAL DE LA CPU
 * Instruccion por instruccion
 * ========================================================================= */

// Flag de ejecucion
int cpu_running = 0;

/* =========================================================================
 * CICLO PRINCIPAL DE LA CPU
 * Instruccion por instruccion
 * ========================================================================= */

void cpu_cycle() {
    // 0. Si la CPU esta apagada, no hacemos nada
    if (!cpu_running) return;

    // 0.1 Chequear INT Harware (como la del DMA)
    // Si hay una pendiente y estan habilitadas, la atendemos
    if (interrupt_pending_dma && cpu_registers.PSW.interrupt_enable) {
        interrupt_pending_dma = 0; // Ya la vimos
        generate_interrupt(INT_IO_DONE);
        return; // Prioridad a la interrupcion
    }

    // 1. FETCH (Busqueda)
    // Buscamos la siguiente instruccion en memoria donde apunte PC
    int pc = cpu_registers.PSW.pc;
    
    // Seguridad para no leer mas alla del fin del mundo
    if (pc >= MEM_SIZE) {
        log_event("ERROR FATAL: El PC se salio de la memoria (%d)!", pc);
        cpu_running = 0; // Detener CPU
        return;
    }

    Word instruction_word = mem_read(pc);
    
    // CHEQUEO DE CENTINELA (END_PROGRAM)
    // Si encontramos el valor magico, detenemos todo.
    if (instruction_word.digits == SENTINEL_VAL) {
        log_event("--- FIN DE PROGRAMA DETECTADO (Sentinel) ---");
        cpu_running = 0; // Apagar motor
        return; 
    }
    
    // Anotamos en la bitacora que hicimos
    log_instruction(pc, "FETCH (Buscando)", instruction_word.digits);

    // Avanzamos el PC para la proxima
    cpu_registers.PSW.pc++;

    // 2. DECODE (Decodificacion)
    // Desarmamos el numero para entender que instruccion es
    int raw = instruction_word.digits;
    
    // Los ultimos 5 son el valor
    cpu_registers.IR.valor = raw % 100000;
    // El del medio es el modo de direccionamiento
    cpu_registers.IR.direccionamiento = (raw / 100000) % 10;
    // Los primeros 2 son el Codigo de Operacion (Opcode)
    cpu_registers.IR.cod_op = (raw / 1000000);
    
    int op = cpu_registers.IR.cod_op;

    // 3. EXECUTE (Ejecucion)
    // Dependiendo del Opcode, llamamos a la funcion que toca
    switch(op) {
        // Aritmética
        case OP_SUM: case OP_RES: case OP_MULT: case OP_DIVI:
            exec_arithmetic(op); 
            break;
            
        // Memoria
        case OP_LOAD: case OP_STR:
            exec_transfer_mem(op); 
            break;
            
        // Registros Especiales
        case OP_LOADRX: cpu_registers.AC = int_to_word(cpu_registers.RX); break;
        case OP_STRRX:  cpu_registers.RX = word_to_int(cpu_registers.AC); break;
        
        // Comparaciones y Saltos
        case OP_COMP:   exec_comp(); break;
        case OP_JMPE: case OP_JMPNE: case OP_JMPLT: case OP_JMPLGT:
            exec_jump(op); 
            break;
            
        // Salto Incondicional
        case OP_J:
            exec_jump(op); 
            break;
            
        // Sistema
        case OP_SVC:
            // Llamada al sistema (System Call)
            generate_interrupt(INT_SVC);
            break;
        case OP_RETRN:
             // Volver de una subrutina o interrupcion
             // Recuperamos CONTEXTO COMPLETO (orden inverso al push)
             // 1. Pop RX
             {
                 cpu_registers.RX = word_to_int(mem_read(cpu_registers.SP));
                 cpu_registers.SP++;
                 
                 // 2. Pop AC
                 cpu_registers.AC = mem_read(cpu_registers.SP);
                 cpu_registers.SP++;
                 
                 // 3. Pop Flags (PSW)
                 int flags = word_to_int(mem_read(cpu_registers.SP));
                 cpu_registers.SP++;
                 
                 // Desempaquetar
                 cpu_registers.PSW.interrupt_enable = flags % 10;
                 cpu_registers.PSW.operation_mode = (flags / 10) % 10;
                 cpu_registers.PSW.condition_code = (flags / 100) % 10;
                 
                 // 4. Pop PC
                 cpu_registers.PSW.pc = word_to_int(mem_read(cpu_registers.SP));
                 cpu_registers.SP++; 
             }
             break;
        case OP_HAB:  cpu_registers.PSW.interrupt_enable = INT_ENABLED; break;
        case OP_DHAB: cpu_registers.PSW.interrupt_enable = INT_DISABLED; break;
        case OP_TTI:
             // Configurar Timer (no implementado completo aun)
             break;
        case OP_CHMOD:
             // Cambiar entre modo Usuario y Kernel
             if (cpu_registers.PSW.operation_mode == MODE_KERNEL) {
                 cpu_registers.PSW.operation_mode = (cpu_registers.PSW.operation_mode == MODE_KERNEL) ? MODE_USER : MODE_KERNEL;
             }
             break;
             
        // Registros Base/Limite
        case OP_LOADRB: cpu_registers.AC = int_to_word(cpu_registers.RB); break;
        case OP_STRRB:  cpu_registers.RB = word_to_int(cpu_registers.AC); break;
        case OP_LOADRL: cpu_registers.AC = int_to_word(cpu_registers.RL); break;
        case OP_STRRL:  cpu_registers.RL = word_to_int(cpu_registers.AC); break;
        case OP_LOADSP: cpu_registers.AC = int_to_word(cpu_registers.SP); break;
        case OP_STRSP:  cpu_registers.SP = word_to_int(cpu_registers.AC); break;
        
        // Pila
        case OP_PSH: case OP_POP:
            exec_stack(op);
            break;
            
        // DMA (Discos)
        case OP_SDMAP: dma.selected_track = cpu_registers.IR.valor; break;
        case OP_SDMAC: dma.selected_cylinder = cpu_registers.IR.valor; break;
        case OP_SDMAS: dma.selected_sector = cpu_registers.IR.valor; break;
        case OP_SDMAIO:dma.io_direction = cpu_registers.IR.valor; break;
        case OP_SDMAM: dma.memory_address = cpu_registers.IR.valor; break;
        case OP_SDMAON: dma_start_transfer(); break; // Arranca el hilo
            
        default:
            log_interrupt(INT_INST_INVALID, "Opcode que no entiendo (Invalido)");
            generate_interrupt(INT_INST_INVALID);
            break;
    }
}
