CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -I. -I./hardware

# Archivos objeto
OBJS = main.o loader.o logger.o \
       hardware/memory.o hardware/cpu.o hardware/dma.o hardware/disk.o

# Nombre del ejecutable
TARGET = machine

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)
	rm -f $(OBJS)

# Regla genérica para construir .o desde .c
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) virtual_disk.bin virtual_machine.log

.PHONY: all clean