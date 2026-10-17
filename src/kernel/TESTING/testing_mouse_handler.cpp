#include "testing_mouse_handler.hpp"

#include "drivers/video/draw.hpp"

extern "C" {
    #include "print.h"
    #include "drivers/video/framebuffer.h"
    #include "log.h"
}

#define CURSOR_SIZE 9
#define CURSOR_HALF (CURSOR_SIZE / 2)

int32_t saved_x = -1;
int32_t saved_y = -1;
uint8_t saved_pixels[CURSOR_SIZE][CURSOR_SIZE][3];

void cursor_restore_saved() {
     if (saved_x < 0 || saved_y < 0)
        return;

    for (int dy = 0; dy < CURSOR_SIZE; dy++) {
        for (int dx = 0; dx < CURSOR_SIZE; dx++) {
            int32_t px = saved_x - CURSOR_HALF + dx;
            int32_t py = saved_y - CURSOR_HALF + dy;

            if (px < 0 || py < 0) continue;

            framebuffer_put_pixel(
                (uint32_t) px, (uint32_t) py,
                saved_pixels[dy][dx][0],
                saved_pixels[dy][dx][1],
                saved_pixels[dy][dx][2]
            );
        }
    }
}

void cursor_save_under(int32_t origin_x, int32_t origin_y) {
    for (int dy = 0; dy < CURSOR_SIZE; dy++) {
        for (int dx = 0; dx < CURSOR_SIZE; dx++) {
            int32_t px = origin_x - CURSOR_HALF + dx;
            int32_t py = origin_y - CURSOR_HALF + dy;

            uint8_t r = 0, g = 0, b = 0;
            if (px >= 0 && py >= 0) {
                framebuffer_get_pixel((uint32_t) px, (uint32_t) py, &r, &g, &b);
            }

            saved_pixels[dy][dx][0] = r;
            saved_pixels[dy][dx][1] = g;
            saved_pixels[dy][dx][2] = b;
        }
    }

    saved_x = origin_x;
    saved_y = origin_y;
}

void cursor_draw_at(int32_t origin_x, int32_t origin_y) {
    cursor_restore_saved();           // erase wherever it was before
    cursor_save_under(origin_x, origin_y); // capture what's under the new spot

    for (int dy = 0; dy < CURSOR_SIZE; dy++) {
        for (int dx = 0; dx < CURSOR_SIZE; dx++) {
            bool is_shape = (dx == CURSOR_HALF) || (dy == CURSOR_HALF);
            if (!is_shape) continue;

            int32_t px = origin_x - CURSOR_HALF + dx;
            int32_t py = origin_y - CURSOR_HALF + dy;
            if (px < 0 || py < 0) continue;

            framebuffer_put_pixel((uint32_t) px, (uint32_t) py, 255, 255, 0);
        }
    }
}

const char* button_name(MouseButton button) {
    switch (button) {
        case MOUSE_BUTTON_LEFT:   return "Left";
        case MOUSE_BUTTON_RIGHT:  return "Right";
        case MOUSE_BUTTON_MIDDLE: return "Middle";
        default:                  return "Unknown";
    }
}

void testing_mouse_handler(MouseEvent event) {
    Circle triangle;
    draw_circle(Point{450, 450}, 100, &triangle);
    fill_circle(&triangle, Color{255, 0, 0});

    switch (event.type) {

        case MOUSE_EVENT_MOVE:
            cursor_draw_at(event.x, event.y);
            break;

        case MOUSE_EVENT_BUTTON_DOWN:
            INF_PRINTF("%C11,0%s click at (%d, %d)%C15,0\n", button_name(event.button), event.x, event.y);
            if (event.button == MOUSE_BUTTON_LEFT) {
                if (point_inside_shape(triangle, Point{(uint64_t)event.x, (uint64_t)event.y})) {
                    println("Clicked inside triangle");
                }
            }
            break;

        case MOUSE_EVENT_SCROLL_UP:
            print_set_color(PRINT_COLOR_LIGHT_CYAN, PRINT_COLOR_BLACK);
            println("Scrolled up");
            print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
            break;

        case MOUSE_EVENT_SCROLL_DOWN:
            print_set_color(PRINT_COLOR_LIGHT_CYAN, PRINT_COLOR_BLACK);
            println("Scrolled down");
            print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
            break;

        case MOUSE_EVENT_BUTTON_UP:
        default:
            break;
    }
}

void testing_mouse_draw_initial() {
    cursor_draw_at(mouse_get_x(), mouse_get_y());
}
