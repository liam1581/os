#pragma once

#include <stdint.h>

extern "C" {
    #include "math.h"
}

struct Point {
    uint64_t x;
    uint64_t y;
};

struct Color {
    uint8_t R;
    uint8_t G;
    uint8_t B;
};

struct Line {
    Point A;
    Point B;

    Color color;
    uint64_t t;

    double length;
};

struct Shape {
    Color color;
    
    uint64_t t;

    double A_area;
    double u;

    virtual bool contains(Point p) const = 0;
    virtual ~Shape() = default;
};

struct Triangle : public Shape {
    Point A, B, C;

    Line AB, BC, CA; // c, a, b

    static double cross(Point a, Point b, Point p) {
        double abx = (double)b.x - (double)a.x;
        double aby = (double)b.y - (double)a.y;
        double apx = (double)p.x - (double)a.x;
        double apy = (double)p.y - (double)a.y;

        return abx * apy - aby * apx;
    }
    bool contains(Point p) const override {
        double c1 = cross(A, B, p);
        double c2 = cross(B, C, p);
        double c3 = cross(C, A, p);

        bool has_negative = c1 < 0.0 || c2 < 0.0 || c3 < 0.0;
        bool has_positive = c1 > 0.0 || c2 > 0.0 || c3 > 0.0;

        return (!has_negative && has_positive);
    }

    double h;
};
struct Cube : public Shape {
    Point A;
    Point B;
    Point C;
    Point D;

    Line AB;
    Line BC;
    Line CD;
    Line DA;

    bool contains(Point p) const override {
        uint64_t min_x = A.x;
        uint64_t max_x = A.x;
        uint64_t min_y = A.y;
        uint64_t max_y = A.y;

        Point corners[] = { B, C, D };

        for (int i = 0; i < 3; i++) {
            if (corners[i].x < min_x) min_x = corners[i].x;
            if (corners[i].x > max_x) max_x = corners[i].x;
            if (corners[i].y < min_y) min_y = corners[i].y;
            if (corners[i].y > max_y) max_y = corners[i].y;
        }

        return p.x >= min_x && p.x <= max_x && p.y >= min_y && p.y <= max_y;
    }
};
struct Circle : public Shape {
    Point center;
    
    double r;
    double d;

    bool contains(Point p) const override {
        double dx = (double)p.x - (double)center.x;
        double dy = (double)p.y - (double)center.y;
        
        return sqr(dx) + sqr(dy) <= sqr(r);
    }
};

inline bool point_inside_shape(const Shape& shape, Point p) {
    return shape.contains(p);
}

inline Color white = { .R=255, .G=255, .B=255 };

double distance_points(Point a, Point b);

void draw_line(Point a, Point b, Line* line_out, uint64_t thickness = 1, Color color = white);

void draw_cube(Point a, Point b, Point c, Point d, Cube* cube_out, uint64_t thickness = 1, Color color = white);
void fill_cube(Cube* cube, Color fill_color);
void fill_cube(Cube* cube);

void draw_triangle(Point a, Point b, Point c, Triangle* triangle_out, uint64_t thickness = 1, Color color = white);
void fill_triangle(Triangle* triangle, Color fill_color);
void fill_triangle(Triangle* triangle);

void draw_circle(Point center, double radius, Circle* circle_out, uint64_t thickness = 1, Color color = white);
void fill_circle(Circle* circle, Color fill_color);
void fill_circle(Circle* circle);