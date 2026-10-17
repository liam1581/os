#pragma once

#include <stdint.h>
#include "bool.h"

typedef enum {
    LMB = 0,
    RMB = 1,
    MMB = 2,
    MOUSE_BUTTON_LEFT = 0,
    MOUSE_BUTTON_RIGHT = 1,
    MOUSE_BUTTON_MIDDLE = 2,
} MouseButton;

typedef enum {
    MOUSE_EVENT_MOVE,
    MOUSE_EVENT_BUTTON_DOWN,
    MOUSE_EVENT_BUTTON_UP,
    MOUSE_EVENT_SCROLL_UP,
    MOUSE_EVENT_SCROLL_DOWN,
} MouseEventType;

typedef struct {
    MouseEventType type;
    int32_t x;
    int32_t y;
    MouseButton button;
} MouseEvent;

void mouse_init();
void mouse_poll();

void mouse_set_handler(void (*handler)(MouseEvent event));

int32_t mouse_get_x();
int32_t mouse_get_y();
bool mouse_button_is_down(MouseButton button);