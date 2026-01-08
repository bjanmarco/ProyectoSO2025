#ifndef HARDWARE_H
#define HARDWARE_H

#include <stdint.h>
#include <pthread.h>

/* =========================================================================
 * 1. CONSTANTES DE ARQUITECTURA
 * ========================================================================= */

typedef struct{
    signed int sign;
    unsigned int digits;
} Word;

typedef struct {
    int condition_code;     // 1 dígito decimal
    int operation_mode;     // 1 dígito decimal (Kernel/User)
    int interrupt_enable;   // 1 dígito decimal
    int pc;
} PSW_t;

typedef struct {
    int cod_op;
    int direccionamiento;
    int valor;
} IR_t;

typedef struct {
    Word AC;     // Acumulador (Propósito general implícito) [cite: 9]
    Word MAR;    // Memory Address Register [cite: 14]
    Word MDR;    // Memory Data Register [cite: 15]
    IR_t IR;     // Instruction Register [cite: 16]
    int RB;     // Registro Base [cite: 17]
    int RL;     // Registro Límite [cite: 17]
    int RX;     // Registro Base de la Pila [cite: 18]
    int SP;     // Puntero de Pila [cite: 19]
    PSW_t PSW;  // Palabra de Estado del Sistema [cite: 20]
} Registers;

// Especificaciones de Memoria [cite: 33, 34]
#define MEM_SIZE        2000    // Tamaño total de la RAM
#define OS_MEM_SIZE     300     // Posiciones reservadas para el SO
#define USER_MEM_START  300     // Inicio de memoria de usuario

// Modos de Operación [cite: 29]
#define MODE_USER       0
#define MODE_KERNEL     1

// Códigos de Condición (CC) [cite: 24, 25, 26, 27]
#define CC_ZERO         0       // Resultado = 0
#define CC_NEGATIVE     1       // Resultado < 0
#define CC_POSITIVE     2       // Resultado > 0
#define CC_OVERFLOW     3       // Desbordamiento

// Interrupciones (Habilitación) [cite: 31]
#define INT_DISABLED    0
#define INT_ENABLED     1

// Tipos de Direccionamiento [cite: 39, 40, 41]
#define ADDR_DIRECT     0
#define ADDR_IMMEDIATE  1
#define ADDR_INDEXED    2

/* =========================================================================
 * 2. CÓDIGOS DE OPERACIÓN (Instruction Set) [cite: 43]
 * ========================================================================= */
// Aritméticas
#define OP_SUM      0
#define OP_RES      1
#define OP_MULT     2
#define OP_DIVI     3

// Transferencia de Datos (Memoria <-> AC)
#define OP_LOAD     4
#define OP_STR      5

// Transferencia de Registros Especiales
#define OP_LOADRX   6
#define OP_STRRX    7

// Comparación y Saltos
#define OP_COMP     8
#define OP_JMPE     9   // Salta si AC == M[SP]
#define OP_JMPNE    10  // Salta si AC != M[SP]
#define OP_JMPLT    11  // Salta si AC < M[SP]
#define OP_JMPLGT   12  // Salta si AC > M[SP]

// Control y Sistema
#define OP_SVC      13  // Llamada al sistema
#define OP_RETRN    14
#define OP_HAB      15  // Habilita interrupciones
#define OP_DHAB     16  // Deshabilita interrupciones
#define OP_TTI      17  // Timer
#define OP_CHMOD    18  // Cambia modo

// Gestión de Registros Base/Límite/Pila
#define OP_LOADRB   19
#define OP_STRRB    20
#define OP_LOADRL   21
#define OP_STRRL    22
#define OP_LOADSP   23
#define OP_STRSP    24

// Pila
#define OP_PSH      25
#define OP_POP      26

// Salto Incondicional
#define OP_J        27

// Operaciones de E/S (DMA)
#define OP_SDMAP    28  // Set Track (Pista)
#define OP_SDMAC    29  // Set Cylinder (Cilindro)
#define OP_SDMAS    30  // Set Sector
#define OP_SDMAIO   31  // Set I/O Mode (0=Read, 1=Write)
#define OP_SDMAM    32  // Set Memory Address
#define OP_SDMAON   33  // Start DMA

/* =========================================================================
 * 3. CÓDIGOS DE INTERRUPCIÓN (Vector de Interrupciones) [cite: 70]
 * ========================================================================= */
#define INT_SVC_INVALID  0
#define INT_CODE_INVALID 1
#define INT_SVC          2
#define INT_TIMER        3
#define INT_IO_DONE      4
#define INT_INST_INVALID 5
#define INT_ADDR_INVALID 6
#define INT_UNDERFLOW    7
#define INT_OVERFLOW     8

/* =========================================================================
 * 4. ESTRUCTURAS DE DATOS DEL HARDWARE
 * ========================================================================= */
#define DISK_TRACKS     10
#define DISK_CYLINDERS  10
#define DISK_SECTORS    100
#define SECTOR_SIZE     9
/**
 * Registros del CPU.
 * Basado en la lista de registros de propósito especial [cite: 13-20].
 */
 
typedef struct {
    int selected_track;
    int selected_cylinder;
    int selected_sector;
    int io_direction;       // 0 = Leer, 1 = Escribir [cite: 53]
    int memory_address;     // Dirección RAM origen/destino [cite: 51]
    int status;             // 0 = Éxito, 1 = Error [cite: 62, 63]
    int is_busy;            // Bandera interna para simulación
} DMA_Controller;

// Memoria RAM: Arreglo de 2000 enteros (palabras) [cite: 33]
extern int main_memory[MEM_SIZE];

// CPU Registers
extern Registers cpu_registers;

// Disco Duro
extern HardDisk hdd;

// DMA
extern DMA_Controller dma;

// Bus del Sistema (Mecanismo de Arbitraje) [cite: 44, 96]
// Usamos un mutex para garantizar exclusión mutua entre CPU y DMA al acceder a RAM.
extern pthread_mutex_t system_bus_lock;

#endif // HARDWARE_H