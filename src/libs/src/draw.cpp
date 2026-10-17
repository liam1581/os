#include "drivers/video/draw.hpp"

extern "C" {
    #include "math.h"
    #include "log.h"

    #include "drivers/video/framebuffer.h"
}

double distance_points(Point a, Point b) {
    double dx = (double)b.x - (double)a.x;
    double dy = (double)b.y - (double)a.y;

    return sqrt(sqr(dx) + sqr(dy));
}

void draw_line(Point a, Point b, Line* line_out, uint64_t thickness, Color color) {
    int dx = b.x - a.x;
    int dy = b.y - a.y;

    int sx = (dx >= 0) ? 1 : -1;
    int sy = (dy >= 0) ? 1 : -1;

    dx = (dx >= 0) ? dx : -dx;
    dy = (dy >= 0) ? dy : -dy;

    int err = dx - dy;

    if (thickness < 1) {
        thickness = 1;
    }

    int radius = thickness / 2;

    while (1) {
        for (int tx = -radius; tx <= radius; tx++) {
            for (int ty = -radius; ty <= radius; ty++) {
                int px = a.x + tx;
                int py = a.y + ty;

                if (px >= 0 && px < framebuffer_get_width() && py >= 0 && py < framebuffer_get_height()) {
                    framebuffer_put_pixel(px, py, color.R, color.G, color.B);
                }
            }
        }

        if (a.x == b.x && a.y == b.y) {
            break;
        }

        int e2 = 2 * err;

        if (e2 > -dy) {
            err -= dy;
            a.x += sx;
        }

        if (e2 < dx) {
            err += dx;
            a.y += sy;
        }
    }

    Line out = {
        .A = a, .B = b,
        .color = color, .t = thickness,
        .length = distance_points(a, b),
    };

    if (line_out != 0) {
        *line_out = out;
    }
}

void draw_cube(Point a, Point b, Point c, Point d, Cube* cube_out, uint64_t thickness, Color color) {
    Line lA;
    Line lB;
    Line lC;
    Line lD;

    draw_line(a, b, &lA, thickness, color);
    draw_line(b, c, &lB, thickness, color);
    draw_line(c, d, &lC, thickness, color);
    draw_line(d, a, &lD, thickness, color);

    double u = (double)(lA.length + lB.length + lC.length + lD.length);

    double s = u / 2;
    double A = sqrt((s - lA.length)*(s - lB.length)*(s - lC.length)*(s - lD.length));

    Cube out;
        out.A = a;
        out.B = b;
        out.C = c;
        out.D = d;
        out.AB = lA;
        out.BC = lB;
        out.CD = lC;
        out.DA = lD;

        out.color = color;
        out.t = thickness;

        out.A_area = A;
        out.u = u;

    if (cube_out != 0) {
        *cube_out = out;
    }
}
void fill_cube(Cube* cube, Color fill_color) {
    if (cube == 0) {
        return;
    }

    uint64_t x_start = (cube->A.x < cube->C.x) ? cube->A.x : cube->C.x;
    uint64_t x_end = (cube->A.x > cube->C.x) ? cube->A.x : cube->C.x;

    uint64_t y_start = (cube->A.y < cube->C.y) ? cube->A.y : cube->C.y;
    uint64_t y_end = (cube->A.y > cube->C.y) ? cube->A.y : cube->C.y;

    uint64_t width = framebuffer_get_width();
    uint64_t height = framebuffer_get_height();

    if (x_start >= width || y_start >= height) {
        return;
    }

    if (x_end >= width) {
        x_end = width - 1;
    }

    if (y_end >= height) {
        y_end = height - 1;
    }

    for (uint64_t y = y_start; y <= y_end; y++) {
        for (uint64_t x = x_start; x <= x_end; x++) {
            framebuffer_put_pixel(x, y, fill_color.R, fill_color.G, fill_color.B);
        }
    }
}
void fill_cube(Cube* cube) {
    fill_cube(cube, cube->color);
}

void draw_triangle(Point a, Point b, Point c, Triangle* triangle_out, uint64_t thickness, Color color) {
    Line lAB;
    Line lBC;
    Line lCA;
    
    draw_line(a, b, &lAB, thickness, color);
    draw_line(b, c, &lBC, thickness, color);
    draw_line(c, a, &lCA, thickness, color);

    double u = lAB.length + lBC.length + lCA.length;
    
    double s = u / 2;
    double A = sqrt(s * (s - lAB.length) * (s - lBC.length) * (s - lCA.length));

    double h = (2 * A) / lAB.length;

    Triangle out;
        out.A = a;
        out.B = b;
        out.C = c;
        out.AB = lAB;
        out.BC = lBC;
        out.CA = lCA;

        out.h = h;

        out.color = color;
        out.t = thickness;
  
        out.A_area = A;
        out.u = u;

    if (triangle_out != 0) {
        *triangle_out = out;
    }
}
void fill_triangle(Triangle* triangle, Color fill_color) {
    uint64_t min_y = triangle->A.y;
    uint64_t max_y = triangle->A.y;

    if (triangle->B.y < min_y) min_y = triangle->B.y;
    if (triangle->C.y < min_y) min_y = triangle->C.y;

    if (triangle->B.y > max_y) max_y = triangle->B.y;
    if (triangle->C.y > max_y) max_y = triangle->C.y;

    for (uint64_t y = min_y; y <= max_y; y++) {
        double intersections[3];
        int count = 0;

        Point points[3] = {
            triangle->A,
            triangle->B,
            triangle->C
        };

        for (int i = 0; i < 3; i++) {
            Point p1 = points[i];
            Point p2 = points[(i + 1) % 3];

            if ((p1.y <= y && p2.y > y) || (p2.y <= y && p1.y > y)) {
                double x = (double)p1.x + ((double)y - (double)p1.y) * ((double)p2.x - (double)p1.x) / ((double)p2.y - (double)p1.y);

                intersections[count++] = x;
            }
        }

        if (count < 2)
            continue;

        double left = intersections[0];
        double right = intersections[1];

        if (left > right) {
            double temp = left;
            left = right;
            right = temp;
        }

        uint64_t x_start = (uint64_t)left;
        uint64_t x_end = (uint64_t)right;

        for (uint64_t x = x_start; x <= x_end; x++) {
            if (x < framebuffer_get_width() && y < framebuffer_get_height()) {
                framebuffer_put_pixel(x, y, fill_color.R, fill_color.G, fill_color.B);
            }
        }
    }
}
void fill_triangle(Triangle* triangle) {
    fill_triangle(triangle, triangle->color);
}

static void put_circle_pixel(int64_t x, int64_t y, Color color) {
    if (x < 0 || y < 0) {
        return;
    }

    if ((uint64_t)x >= framebuffer_get_width() || (uint64_t)y >= framebuffer_get_height()) {
        return;
    }

    framebuffer_put_pixel((uint64_t)x, (uint64_t)y, color.R, color.G, color.B);
}
static void draw_circle_points(int64_t cx, int64_t cy, int64_t x, int64_t y, Color color) {
    put_circle_pixel(cx + x, cy + y, color);
    put_circle_pixel(cx - x, cy + y, color);
    put_circle_pixel(cx + x, cy - y, color);
    put_circle_pixel(cx - x, cy - y, color);

    put_circle_pixel(cx + y, cy + x, color);
    put_circle_pixel(cx - y, cy + x, color);
    put_circle_pixel(cx + y, cy - x, color);
    put_circle_pixel(cx - y, cy - x, color);
}
static void draw_circle_outline(int64_t cx, int64_t cy, int64_t radius, Color color) {
    int64_t x = radius;
    int64_t y = 0;
    int64_t error = 1 - x;

    while (x >= y) {
        draw_circle_points(cx, cy, x, y, color);

        y++;

        if (error < 0) {
            error += 2 * y + 1;
        } else {
            x--;
            error += 2 * (y - x) + 1;
        }
    }
}

void draw_circle(Point center, double r, Circle* circle_out, uint64_t thickness, Color color) {
    if (r < 0.0 || r > (double)INT64_MAX || center.x > INT64_MAX || center.y > INT64_MAX) {
        return;
    }

    if (thickness < 1) {
        thickness = 1;
    }

    if (circle_out != nullptr) {
        circle_out->center = center;
        circle_out->r = r;
        circle_out->d = r * 2;
        circle_out->u = 2 * PI * r;
        circle_out->A_area = PI * sqr(r);
        circle_out->t = thickness;
        circle_out->color = color;
    }

    int64_t cx = (int64_t)center.x;
    int64_t cy = (int64_t)center.y;

    // The midpoint algorithm uses integer radii.
    int64_t outer_radius = (int64_t)(r + 0.5);

    // Draw inward to create the outline thickness.
    for (uint64_t i = 0; i < thickness && (int64_t)i <= outer_radius; i++) {
        draw_circle_outline(cx, cy, outer_radius - (int64_t)i, color);
    }
}
void fill_circle(Circle* circle, Color fill_color)
{
    if (circle == nullptr || circle->r < 0.0) {
        return;
    }

    int64_t cx = (int64_t)circle->center.x;
    int64_t cy = (int64_t)circle->center.y;
    int64_t r = (int64_t)(circle->r + 0.5);

    uint64_t screen_width = framebuffer_get_width();
    uint64_t screen_height = framebuffer_get_height();

    for (int64_t y = -r; y <= r; y++) {
        double remaining = (double)r * r - (double)y * y;
        int64_t x_span = (int64_t)sqrt(remaining);

        int64_t py = cy + y;

        if (py < 0 || (uint64_t)py >= screen_height) {
            continue;
        }

        int64_t x_start = cx - x_span;
        int64_t x_end = cx + x_span;

        if (x_start < 0) {
            x_start = 0;
        }

        if (x_end >= (int64_t)screen_width) {
            x_end = (int64_t)screen_width - 1;
        }

        for (int64_t x = x_start; x <= x_end; x++) {
            framebuffer_put_pixel((uint64_t)x, (uint64_t)py, fill_color.R, fill_color.G, fill_color.B);
        }
    }
}
void fill_circle(Circle* circle) {
    fill_circle(circle, circle->color);
}