#pragma once
#include <stdint.h>

#define SERIAL_BAUD_115200 115200
#define SERIAL_BAUD_57600 57600
#define SERIAL_BAUD_38400 38400
#define SERIAL_BAUD_19200 19200
#define SERIAL_BAUD_9600 9600

void serial_init(uint32_t baud);
void serial_write_byte(uint8_t byte);
void serial_write(const char* str);
void serial_writeln(const char* str);
void serial_close();

uint8_t serial_read_byte();