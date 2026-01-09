#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "logger.h"

static FILE *log_file = NULL;

void logger_init(const char *filename) {
    log_file = fopen(filename, "w");
    if (!log_file) {
        perror("Error al abrir archivo de log");
    }
}

void logger_close() {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

void log_event(const char *format, ...) {
    if (!log_file) return;

    va_list args;
    
    // Timestamp simple
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(log_file, "[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);

    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    fprintf(log_file, "\n");
    fflush(log_file); // Asegurar escritura
}

void log_interrupt(int code, const char *description) {
    // Imprimir en Log
    log_event("INTERRUPCION Generada: Codigo %d - %s", code, description);
    
    // Imprimir en Salida Estándar (Consola) como pide el requerimiento
    printf("\n!!! INTERRUPCION: Codigo %d - %s !!!\n", code, description);
}

void log_instruction(int pc, const char *mnemonic, int operand) {
    log_event("Ejecutando [PC: %05d]: %s %05d", pc, mnemonic, operand);
}
