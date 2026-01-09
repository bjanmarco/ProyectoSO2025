#ifndef LOGGER_H
#define LOGGER_H

// Inicializa el sistema de logs (abre el archivo)
void logger_init(const char *filename);

// Cierra el archivo de log
void logger_close();

// Registra un mensaje en el log (y opcionalmente en stdout)
void log_event(const char *format, ...);

// Registra una interrupción (Log + Stdout obligatoriamente)
void log_interrupt(int code, const char *description);

// Registra una instrucción ejecutada (para debug)
void log_instruction(int pc, const char *mnemonic, int operand);

#endif // LOGGER_H
