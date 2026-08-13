#include "x86_64/pit.h"
#include "x86_64/port.h"

#define PIT_CHANNEL0_DATA 0x40
#define PIT_COMMAND        0x43

#define PIT_BASE_FREQUENCY 1193182u

#define PIT_CMD_CHANNEL0          (0b00 << 6)
#define PIT_CMD_ACCESS_LOHI       (0b11 << 4)
#define PIT_CMD_MODE3_SQUARE_WAVE (0b011 << 1)
#define PIT_CMD_BINARY            (0 << 0)

void pit_init(uint32_t frequency_hz) {
    if (frequency_hz == 0) frequency_hz = 1;

    uint32_t reload = PIT_BASE_FREQUENCY / frequency_hz;
    if (reload == 0) reload = 1;
    if (reload > 0xFFFF) reload = 0xFFFF; // channel 0's reload register is 16-bit

    port_outb(PIT_COMMAND, PIT_CMD_CHANNEL0 | PIT_CMD_ACCESS_LOHI | PIT_CMD_MODE3_SQUARE_WAVE | PIT_CMD_BINARY);
    port_wait();

    port_outb(PIT_CHANNEL0_DATA, (uint8_t)(reload & 0xFF));
    port_wait();
    port_outb(PIT_CHANNEL0_DATA, (uint8_t)((reload >> 8) & 0xFF));
    port_wait();
}
