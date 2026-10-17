#pragma once

extern "C" {
    #include "drivers/mouse/mouse.h"
}

void testing_mouse_handler(MouseEvent event);
void testing_mouse_draw_initial();
