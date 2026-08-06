#include <stddef.h>

#include "drivers/serial.h"
#include "x86_64/port.h"

// COM1 ports
#define SERIAL_COM1_BASE        0x3F8
#define SERIAL_DATA             (SERIAL_COM1_BASE + 0)  // r/w data (DLAB=0)
#define SERIAL_INT_ENABLE       (SERIAL_COM1_BASE + 1)  // interrupt enable (DLAB=0)
#define SERIAL_BAUD_LOW         (SERIAL_COM1_BASE + 0)  // baud divisor low byte (DLAB=1)
#define SERIAL_BAUD_HIGH        (SERIAL_COM1_BASE + 1)  // baud divisor high byte (DLAB=1)
#define SERIAL_FIFO_CTRL        (SERIAL_COM1_BASE + 2)  // FIFO control
#define SERIAL_LINE_CTRL        (SERIAL_COM1_BASE + 3)  // line control
#define SERIAL_MODEM_CTRL       (SERIAL_COM1_BASE + 4)  // modem control
#define SERIAL_LINE_STATUS      (SERIAL_COM1_BASE + 5)  // line status

// Line control bits
#define SERIAL_LINE_8N1         0x03  // 8 data bits, no parity, 1 stop bit
#define SERIAL_DLAB             0x80  // divisor latch access bit

// Line status bits
#define SERIAL_TX_EMPTY         0x20  // transmit buffer empty, ready to send
#define SERIAL_RX_READY         0x01  // data ready to read

// Base clock rate of the 8250 UART
#define SERIAL_BASE_CLOCK       115200

void serial_init(uint32_t baud) {
    uint16_t divisor = (uint16_t)(SERIAL_BASE_CLOCK / baud);

    // Disable interrupts
    port_outb(SERIAL_INT_ENABLE, 0x00);

    // Enable DLAB to set baud rate divisor
    port_outb(SERIAL_LINE_CTRL, SERIAL_DLAB);
    port_outb(SERIAL_BAUD_LOW,  (uint8_t)(divisor & 0xFF));
    port_outb(SERIAL_BAUD_HIGH, (uint8_t)((divisor >> 8) & 0xFF));

    // Disable DLAB, set 8N1 mode
    port_outb(SERIAL_LINE_CTRL, SERIAL_LINE_8N1);

    // Enable and clear FIFO, 14-byte threshold
    port_outb(SERIAL_FIFO_CTRL, 0xC7);

    // Enable DTR, RTS, and OUT2 (required to enable IRQs on real hardware)
    port_outb(SERIAL_MODEM_CTRL, 0x0B);
}

void serial_write_byte(uint8_t byte) {
    // Busy-wait until transmit buffer is empty
    while (!(port_inb(SERIAL_LINE_STATUS) & SERIAL_TX_EMPTY));
    port_outb(SERIAL_DATA, byte);
}

void serial_write(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        serial_write_byte((uint8_t)str[i]);
    }
}

void serial_writeln(const char* str) {
    serial_write(str);
    serial_write("\r\n");
}

void serial_close() {
    while (!(port_inb(SERIAL_LINE_STATUS) & SERIAL_TX_EMPTY));
    port_outb(SERIAL_INT_ENABLE, 0x00);
    port_outb(SERIAL_FIFO_CTRL, 0x00);
    port_outb(SERIAL_MODEM_CTRL, 0x00);
}

uint8_t serial_read_byte() {
    // Busy-wait until data is available
    while (!(port_inb(SERIAL_LINE_STATUS) & SERIAL_RX_READY));
    return port_inb(SERIAL_DATA);
}