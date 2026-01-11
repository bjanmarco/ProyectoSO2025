#ifndef HARDWARE_H
#define HARDWARE_H

#include <stdint.h>
#include <pthread.h>

/* =========================================================================
 * 1. CONSTANTES DE ARQUITECTURA
 * ========================================================================= */

// Estructura de Palabra (8 dígitos decimales totales)
// Formato: [Signo (1d)] [Magnitud (7d)]
// Signo: 0 = Positivo (+), 1 = Negativo (-)
typedef struct{
    int sign;      // 1er dígito
    int digits;    // 7 dígitos restantes (Magnitud)
} Word;

// Detalle del Registro PSW (Palabra de Estado)
// Estructura: [CC (1d)] [Modo (1d)] [Int (1d)] [PC (5d)]
typedef struct {
    int condition_code;     // CC: 0(=), 1(<), 2(>), 3(Overflow)
    int operation_mode;     // Modo: 0=Usuario, 1=Kernel
    int interrupt_enable;   // Int: 0=Disabled, 1=Enabled
    int pc;                 // PC: Dirección siguiente instrucción (5d)
} PSW_t;

// Formato de Instrucción (8 dígitos)
// Estructura: [OpCode (2d)] [Direccionamiento (1d)] [Valor (5d)]
typedef struct {
    int cod_op;             // 2 dígitos
    int direccionamiento;   // 1 dígito: 0=Dir, 1=Inm, 2=Idx
    int valor;              // 5 dígitos
} IR_t;

typedef struct {
    Word AC;      // Acumulador (Datos/Aritmética)
    Word MAR;     // Memory Address Register
    Word MDR;     // Memory Data Register
    IR_t IR;      // Instruction Register (Instrucción actual)
    int RB;       // Registro Base (Gestión Memoria)
    int RL;       // Registro Límite (Gestión Memoria)
    int RX;       // Registro Base de la Pila (Indexado también usa esto)
    int SP;       // Puntero Tope de Pila
    PSW_t PSW;    // Palabra de Estado del Sistema
} Registers;

// Especificaciones de Memoria (RAM)
#define MEM_SIZE        2000    // Capacidad: 2000 palabras
#define OS_MEM_SIZE     300     // 0000 - 0299: Sistema Operativo
#define USER_MEM_START  300     // 0300 - 1999: Espacio de Usuario

// Modos de Ejecución
#define MODE_USER       0
#define MODE_KERNEL     1

// Códigos de Condición (CC)
#define CC_ZERO         0       // = (Igual a 0)
#define CC_NEGATIVE     1       // < (Menor que 0)
#define CC_POSITIVE     2       // > (Mayor que 0)
#define CC_OVERFLOW     3       // Desbordamiento

// Interrupciones (Estado)
#define INT_DISABLED    0
#define INT_ENABLED     1

// Tipos de Direccionamiento
#define ADDR_DIRECT     0       // Referencia a memoria
#define ADDR_IMMEDIATE  1       // Valor es el dato
#define ADDR_INDEXED    2       // Índice desde AC + memoria (o RX + valor según diseño CPU)

/* =========================================================================
 * 2. CÓDIGOS DE OPERACIÓN (Instruction Set)
 * ========================================================================= */
// Aritméticas
#define OP_SUM      0
#define OP_RES      1
#define OP_MULT     2
#define OP_DIVI     3

// Transferencia de Datos
#define OP_LOAD     4
#define OP_STR      5

// Transferencia de Registros Especiales
#define OP_LOADRX   6
#define OP_STRRX    7

// Comparación y Saltos
#define OP_COMP     8
#define OP_JMPE     9   // Jump if Equal
#define OP_JMPNE    10  // Jump if Not Equal
#define OP_JMPLT    11  // Jump if Less Than
#define OP_JMPLGT   12  // Jump if Greater Than

// Control y Sistema
#define OP_SVC      13  // Llamada al sistema (System Call)
#define OP_RETRN    14  
#define OP_HAB      15  // Habilita interrupciones
#define OP_DHAB     16  // Deshabilita interrupciones
#define OP_TTI      17  // Timer
#define OP_CHMOD    18  // Cambia modo (Usuario <-> Kernel)

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
 * 3. VECTOR DE INTERRUPCIONES (Códigos 0-8)
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
 * 4. ESTRUCTURAS DE DATOS DE E/S
 * ========================================================================= */
#define DISK_CYLINDERS 10
#define DISK_TRACKS    10
#define DISK_SECTORS   100
#define SECTOR_SIZE    9

// Estructura del Disco Duro
typedef struct {
    char data[SECTOR_SIZE]; 
} Sector;

typedef struct {
    // Arreglo 3D [Cilindro][Pista][Sector]
    Sector sectors[DISK_CYLINDERS][DISK_TRACKS][DISK_SECTORS];
} HardDisk;

// Controlador DMA
typedef struct {
    // Registros de Control
    int selected_track;
    int selected_cylinder;
    int selected_sector;
    int io_direction;       // 0 = Leer, 1 = Escribir
    int memory_address;     // Dirección RAM
    
    // Estado
    int status;             // 0 = Éxito, 1 = Error
    int is_busy;            // 1 = Operación en curso
    
    // Hilo para operación asíncrona
    pthread_t thread_id;
} DMA_Controller;

/* =========================================================================
 * 5. VARIABLES GLOBALES (Componentes de Hardware)
 * ========================================================================= */

// Memoria RAM: Arreglo de 2000 Palabras
extern Word main_memory[MEM_SIZE];

// CPU Registers
extern Registers cpu_registers;

// Flag Global de Interrupciones Pendientes
// Sencillo: 1 = Interrupción Pendiente, 0 = Nada
// En un hardware real esto serían líneas físicas hacia la CPU.
extern int interrupt_pending_dma; // Línea de interrupción del DMA (INT 4)


// Disco Duro
extern HardDisk hdd;

// DMA
extern DMA_Controller dma;

// Bus del Sistema (Mutex)
#include <semaphore.h>

/* ... (omitted constants) ... */

// Bus del Sistema (Semáforo para arbitraje)
// Usamos un semáforo binario (valor 1) para controlar quién usa el bus.
extern sem_t system_bus_lock;

// VALOR CENTINELA: FF FF FF FF (-1)
// Se usa para marcar el fin del programa y detener la CPU.
#define WORD_SENTINEL_SIGN   1
#define WORD_SENTINEL_DIGITS 9999999 // Usaremos -9999999 como convención simple interna o mejor:
// Dado que Word tiene signo y digitos separados, definamos un valor unico imposible.
// El simulador parsea digitos como positivos.
// Pero la memoria es de Words.
// Vamos a reusar un valor que no sea una instruccion valida.
// Opcode 99 no existe.
#define SENTINEL_VAL 99999999 

/* =========================================================================
 * 6. API DE HARDWARE
 * ========================================================================= */

// Flag para saber si la CPU sigue corriendo
extern int cpu_running;

// Inicialización
void hardware_init();
void hardware_shutdown();
void memory_init();
void disk_init();
void disk_save();

// Memoria
void mem_write(int address, Word data);
Word mem_read(int address);

// CPU
void cpu_cycle();       // Ejecuta fetch-decode-execute
void cpu_reset();       // Reinicia registros
int word_to_int(Word w);
Word int_to_word(int val);

// Disco / DMA
void dma_start_transfer(); 
int dma_check_interrupt(); 

#endif // HARDWARE_H