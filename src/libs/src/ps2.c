#include "x86_64/port.h"

#define PORT_PS2_DATA    0x60
#define PORT_PS2_STATUS  0x64

#define PS2_STATUS_OUTPUT_FULL (1 << 0)

uint8_t ps2_read_scan_code() {
    return port_inb(PORT_PS2_DATA);
}

void ps2_flush_output_buffer() {
    for (int i = 0; i < 64; i++) {
        uint8_t status = port_inb(PORT_PS2_STATUS);

        if ((status & PS2_STATUS_OUTPUT_FULL) == 0) {
            break;
        }

        port_inb(PORT_PS2_DATA);
    }
}