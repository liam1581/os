#pragma once

#include <stdint.h>

uint8_t port_inb(uint16_t port);
void port_outb(uint16_t port, uint8_t value);
uint16_t port_inw(uint16_t port);
void port_outw(uint16_t port, uint16_t value);
void port_wait();