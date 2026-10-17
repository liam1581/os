#include "drivers/mouse/mouse.h"

#include "x86_64/port.h"
#include "x86_64/idt.h"
#include "x86_64/ps2.h"
#include "drivers/video/framebuffer.h"

#include <stddef.h>

#define PORT_PS2_DATA    0x60
#define PORT_PS2_STATUS  0x64
#define PORT_PS2_COMMAND 0x64

#define PS2_STATUS_OUTPUT_FULL (1 << 0)
#define PS2_STATUS_INPUT_FULL  (1 << 1)

#define MOUSE_EVENT_QUEUE_SIZE 64

static bool has_wheel = false;

static uint8_t packet[4];
static int packet_index = 0;
static int packet_size = 3;

static int32_t mouse_x = 0;
static int32_t mouse_y = 0;
static uint8_t button_state = 0;      // bit0=left, bit1=right, bit2=middle
static uint8_t prev_button_state = 0;

static void (*user_handler)(MouseEvent event) = NULL;

// Written only by the IRQ handler, read by mouse_poll() -- and vice
// versa for queue_tail. A single-producer/single-consumer ring buffer
// like this needs no locking as long as each index has exactly one
// writer; volatile just keeps the compiler from caching/reordering
// either side's view of them.
static volatile MouseEvent event_queue[MOUSE_EVENT_QUEUE_SIZE];
static volatile uint32_t queue_head = 0;
static volatile uint32_t queue_tail = 0;

static bool ps2_wait_can_write(void) {
    for (int i = 0; i < 100000; i++) {
        if ((port_inb(PORT_PS2_STATUS) & PS2_STATUS_INPUT_FULL) == 0)
            return true;
    }
    return false;
}

static bool ps2_wait_can_read(void) {
    for (int i = 0; i < 100000; i++) {
        if (port_inb(PORT_PS2_STATUS) & PS2_STATUS_OUTPUT_FULL)
            return true;
    }
    return false;
}

static void ps2_controller_write(uint8_t command) {
    ps2_wait_can_write();
    port_outb(PORT_PS2_COMMAND, command);
}

static void ps2_data_write(uint8_t data) {
    ps2_wait_can_write();
    port_outb(PORT_PS2_DATA, data);
}

static uint8_t ps2_data_read(void) {
    ps2_wait_can_read();
    return port_inb(PORT_PS2_DATA);
}

// Sends a byte to the mouse (the controller's second PS/2 port) and
// reads back its reply -- a real mouse ACKs with 0xFA, but the result
// is only checked loosely (mostly just drained) during init below,
// since a hobby-grade init sequence like this one gains little from
// being strict about it and it keeps the code simpler.
static uint8_t mouse_write_and_read_reply(uint8_t data) {
    ps2_controller_write(0xD4); // "next byte on 0x60 is for the 2nd PS/2 port"
    ps2_data_write(data);
    return ps2_data_read();
}

static void queue_push(MouseEvent event) {
    uint32_t next = (queue_head + 1) % MOUSE_EVENT_QUEUE_SIZE;

    if (next == queue_tail)
        return; // queue full -- drop rather than overwrite unread events

    event_queue[queue_head] = event;
    queue_head = next;
}

// The IRQ12 handler. Deliberately does no font/pixel drawing -- same
// constraint as everywhere else interrupt handlers touch shared state
// in this kernel (see fbcursor_tick()'s comment in ram_watchdog.c):
// this only ever assembles packet bytes and pushes pure-data events.
// Actual cursor drawing/printing happens in mouse_poll(), called from
// normal (non-interrupt) context.
static void mouse_irq_handler(void) {
    uint8_t byte = port_inb(PORT_PS2_DATA);

    // Byte 0 of every packet always has bit 3 set -- if that's not
    // what we're looking at while expecting a fresh packet, the
    // stream is desynced (e.g. a byte got dropped somewhere); drop
    // this byte and wait for a byte that actually looks like a
    // packet start, rather than building a garbage packet out of it.
    if (packet_index == 0 && (byte & 0x08) == 0) {
        return;
    }

    packet[packet_index++] = byte;

    if (packet_index < packet_size) {
        return;
    }

    packet_index = 0;

    uint8_t flags = packet[0];

    bool x_overflow = flags & 0x40;
    bool y_overflow = flags & 0x80;

    int32_t dx = x_overflow ? 0 : (int32_t)(int8_t) packet[1];
    int32_t dy = y_overflow ? 0 : (int32_t)(int8_t) packet[2];

    // The PS/2 mouse's Y axis increases upward; screen Y increases
    // downward.
    dy = -dy;

    mouse_x += dx;
    mouse_y += dy;

    int32_t max_x = (int32_t) framebuffer_get_width() - 1;
    int32_t max_y = (int32_t) framebuffer_get_height() - 1;
    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;

    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x > max_x) mouse_x = max_x;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y > max_y) mouse_y = max_y;

    prev_button_state = button_state;
    button_state = flags & 0x07; // bit0=left, bit1=right, bit2=middle

    if (dx != 0 || dy != 0) {
        MouseEvent event = { MOUSE_EVENT_MOVE, mouse_x, mouse_y, MOUSE_BUTTON_LEFT };
        queue_push(event);
    }

    for (int i = 0; i < 3; i++) {
        uint8_t mask = (uint8_t) (1 << i);
        bool was_down = prev_button_state & mask;
        bool is_down = button_state & mask;

        if (is_down && !was_down) {
            MouseEvent event = { MOUSE_EVENT_BUTTON_DOWN, mouse_x, mouse_y, (MouseButton) i };
            queue_push(event);
        } else if (!is_down && was_down) {
            MouseEvent event = { MOUSE_EVENT_BUTTON_UP, mouse_x, mouse_y, (MouseButton) i };
            queue_push(event);
        }
    }

    if (has_wheel) {
        int8_t scroll = (int8_t) packet[3];

        // IntelliMouse convention: negative Z = wheel rotated away
        // from the user (scroll up/forward), positive = toward the
        // user (scroll down/backward). Flip this if it reads
        // backwards on your hardware/VM -- wheel direction convention
        // isn't universally consistent across implementations.
        if (scroll < 0) {
            MouseEvent event = { MOUSE_EVENT_SCROLL_UP, mouse_x, mouse_y, MOUSE_BUTTON_LEFT };
            queue_push(event);
        } else if (scroll > 0) {
            MouseEvent event = { MOUSE_EVENT_SCROLL_DOWN, mouse_x, mouse_y, MOUSE_BUTTON_LEFT };
            queue_push(event);
        }
    }
}

void mouse_init(void) {
    ps2_flush_output_buffer();

    // Enable the auxiliary (mouse) device.
    ps2_controller_write(0xA8);

    // Read-modify-write the controller's configuration byte: enable
    // IRQ12 (bit 1) and make sure the second port's clock isn't
    // disabled (bit 5).
    ps2_controller_write(0x20);
    uint8_t config = ps2_data_read();
    config |= (1 << 1);
    config &= (uint8_t) ~(1 << 5);
    ps2_controller_write(0x60);
    ps2_data_write(config);

    // Reset the mouse to its power-on defaults.
    mouse_write_and_read_reply(0xF6);

    // Standard "magic sequence" to switch a PS/2 mouse into
    // IntelliMouse (4-byte packet, scroll wheel) mode: set the sample
    // rate to 200, then 100, then 80 in a row. A real wheel mouse
    // (and QEMU's/VirtualBox's emulated one) recognizes this and
    // starts including a 4th, scroll-delta byte in every packet from
    // then on. A plain 3-button mouse just... sets its sample rate to
    // 80 and ignores the significance, harmless either way.
    mouse_write_and_read_reply(0xF3);
    mouse_write_and_read_reply(200);
    mouse_write_and_read_reply(0xF3);
    mouse_write_and_read_reply(100);
    mouse_write_and_read_reply(0xF3);
    mouse_write_and_read_reply(80);

    // Device ID 0x03 means the sequence above worked and 4-byte
    // packets are now in effect.
    mouse_write_and_read_reply(0xF2);
    uint8_t device_id = ps2_data_read();
    has_wheel = (device_id == 0x03);
    packet_size = has_wheel ? 4 : 3;

    // Start streaming movement/button packets.
    mouse_write_and_read_reply(0xF4);

    mouse_x = (int32_t) (framebuffer_get_width() / 2);
    mouse_y = (int32_t) (framebuffer_get_height() / 2);

    packet_index = 0;
    queue_head = 0;
    queue_tail = 0;

    idt_set_handler_mouse(mouse_irq_handler);
}

void mouse_poll(void) {
    while (queue_tail != queue_head) {
        MouseEvent event = event_queue[queue_tail];
        queue_tail = (queue_tail + 1) % MOUSE_EVENT_QUEUE_SIZE;

        if (user_handler != NULL) {
            user_handler(event);
        }
    }
}

void mouse_set_handler(void (*handler)(MouseEvent event)) {
    user_handler = handler;
}

int32_t mouse_get_x(void) {
    return mouse_x;
}

int32_t mouse_get_y(void) {
    return mouse_y;
}

bool mouse_button_is_down(MouseButton button) {
    return (button_state & (1 << (int) button)) != 0;
}
