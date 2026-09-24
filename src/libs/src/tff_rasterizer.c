#include "drivers/files/os/ttf_rasterizer.h"

#include <stddef.h>

typedef struct {
    float* accum;
    uint32_t width;
    uint32_t height;
} AccumBuffer;

// Rasterizes one line segment (already in pixel space) into the
// accumulation buffer, using exact analytic coverage per scanline.
static void accum_line(AccumBuffer* buf, float x0, float y0, float x1, float y1) {
    if (y0 == y1) return;

    float dir = 1.0f;
    if (y0 > y1) {
        float tx = x0, ty = y0;
        x0 = x1; y0 = y1;
        x1 = tx; y1 = ty;
        dir = -1.0f;
    }

    float dxdy = (x1 - x0) / (y1 - y0);
    float x = x0;

    int32_t iy0 = (int32_t)y0;
    if ((float)iy0 > y0) iy0--;

    int32_t iy1_excl = (int32_t)y1;
    if ((float)iy1_excl < y1) iy1_excl++;

    for (int32_t y = iy0; y < iy1_excl; y++) {
        float row_top = (float)y > y0 ? (float)y : y0;
        float row_bot = (float)(y + 1) < y1 ? (float)(y + 1) : y1;

        if (y < 0 || (uint32_t)y >= buf->height) {
            if (row_bot > row_top) x += dxdy * (row_bot - row_top);
            continue;
        }

        if (row_bot <= row_top) continue;

        float dy = row_bot - row_top;
        float x_start = x;
        float x_end = x + dxdy * dy;
        x = x_end;

        float xa = x_start < x_end ? x_start : x_end;
        float xb = x_start < x_end ? x_end : x_start;

        if (xb < 0.0f) xb = 0.0f;
        if (xa < 0.0f) xa = 0.0f;
        if (xa > (float)buf->width) xa = (float)buf->width;
        if (xb > (float)buf->width) xb = (float)buf->width;

        int32_t ixa = (int32_t)xa;
        int32_t ixb = (int32_t)xb;

        float* row = buf->accum + (uint32_t)y * buf->width;

        if (ixa == ixb) {
            if ((uint32_t)ixa < buf->width) {
                float mid_x = (xa + xb) * 0.5f;
                float cover = (float)(ixa + 1) - mid_x;
                row[ixa] += dir * dy * cover;
                if ((uint32_t)(ixa + 1) < buf->width) {
                    row[ixa + 1] += dir * dy * (1.0f - cover);
                }
            }
        } else {
            // Segment spans multiple pixel columns this row: split at
            // each column boundary, accumulating each piece's exact
            // trapezoidal area. seg_y is derived directly from x via
            // the original (pre-min/max-sorted) start/end mapping.
            float prev_x = xa;
            float prev_y = (x_end != x_start)
                ? row_top + (xa - x_start) / (x_end - x_start) * dy
                : row_bot;
            if (prev_y > row_bot) prev_y = row_bot;
            if (prev_y < row_top) prev_y = row_top;

            for (int32_t ix = ixa; ix <= ixb && (uint32_t)ix < buf->width; ix++) {
                float col_right = (float)(ix + 1);
                float seg_x = col_right < xb ? col_right : xb;
                if (seg_x < prev_x) seg_x = prev_x;

                float seg_y = (x_end != x_start)
                    ? row_top + (seg_x - x_start) / (x_end - x_start) * dy
                    : row_bot;
                if (seg_y > row_bot) seg_y = row_bot;
                if (seg_y < row_top) seg_y = row_top;

                float seg_dy = seg_y - prev_y;
                if (seg_dy < 0.0f) seg_dy = -seg_dy;

                float mid_x = (prev_x + seg_x) * 0.5f;
                float cover = col_right - mid_x;
                if (cover < 0.0f) cover = 0.0f;
                if (cover > 1.0f) cover = 1.0f;

                row[ix] += dir * seg_dy * cover;
                if ((uint32_t)(ix + 1) < buf->width) {
                    row[ix + 1] += dir * seg_dy * (1.0f - cover);
                }

                prev_x = seg_x;
                prev_y = seg_y;
            }
        }
    }
}

static void flatten_quad(AccumBuffer* buf, float x0, float y0, float cx, float cy, float x1, float y1) {
    float dx = x1 - x0, dy = y1 - y0;
    float chord = dx * dx + dy * dy;
    float cdx = cx - x0, cdy = cy - y0;
    float ext = cdx * cdx + cdy * cdy;
    float rough = chord > ext ? chord : ext;

    int segments = 4;
    if (rough > 16.0f)   segments = 8;
    if (rough > 64.0f)   segments = 16;
    if (rough > 256.0f)  segments = 24;
    if (rough > 1024.0f) segments = 32;

    float px = x0, py = y0;
    for (int i = 1; i <= segments; i++) {
        float t = (float)i / (float)segments;
        float mt = 1.0f - t;
        float bx = mt * mt * x0 + 2.0f * mt * t * cx + t * t * x1;
        float by = mt * mt * y0 + 2.0f * mt * t * cy + t * t * y1;
        accum_line(buf, px, py, bx, by);
        px = bx;
        py = by;
    }
}

typedef unsigned char bool_t;

static void transform(const TTFGlyphOutline* o, uint16_t i, float scale, float origin_x, float origin_y, float* out_x, float* out_y) {
    *out_x = o->points[i].x * scale + origin_x;
    *out_y = origin_y - o->points[i].y * scale; // flip Y: font space is Y-up, pixel space is Y-down
}

static void rasterize_contour(AccumBuffer* buf, const TTFGlyphOutline* o, uint16_t start, uint16_t end, float scale, float origin_x, float origin_y) {
    uint16_t count = (uint16_t)(end - start + 1);
    if (count < 2) return;

    float start_x, start_y;
    uint16_t first_on_curve_offset = 0;
    bool_t found_on_curve_start = 0;

    for (uint16_t k = 0; k < count; k++) {
        if (o->points[start + k].on_curve) {
            first_on_curve_offset = k;
            found_on_curve_start = 1;
            break;
        }
    }

    if (found_on_curve_start) {
        transform(o, (uint16_t)(start + first_on_curve_offset), scale, origin_x, origin_y, &start_x, &start_y);
    } else {
        float ax, ay, bx, by;
        transform(o, start, scale, origin_x, origin_y, &ax, &ay);
        transform(o, (uint16_t)(start + 1), scale, origin_x, origin_y, &bx, &by);
        start_x = (ax + bx) * 0.5f;
        start_y = (ay + by) * 0.5f;
        first_on_curve_offset = 0;
    }

    float cur_x = start_x, cur_y = start_y;
    bool_t have_pending_control = 0;
    float pending_cx = 0, pending_cy = 0;

    for (uint16_t step = 1; step <= count; step++) {
        uint16_t idx = (uint16_t)(start + (first_on_curve_offset + step) % count);
        float px, py;
        transform(o, idx, scale, origin_x, origin_y, &px, &py);
        bool_t on_curve = (bool_t)o->points[idx].on_curve;

        if (on_curve) {
            if (have_pending_control) {
                flatten_quad(buf, cur_x, cur_y, pending_cx, pending_cy, px, py);
                have_pending_control = 0;
            } else {
                accum_line(buf, cur_x, cur_y, px, py);
            }
            cur_x = px; cur_y = py;
        } else {
            if (have_pending_control) {
                float mid_x = (pending_cx + px) * 0.5f;
                float mid_y = (pending_cy + py) * 0.5f;
                flatten_quad(buf, cur_x, cur_y, pending_cx, pending_cy, mid_x, mid_y);
                cur_x = mid_x; cur_y = mid_y;
            }
            pending_cx = px; pending_cy = py;
            have_pending_control = 1;
        }
    }

    if (have_pending_control) {
        flatten_quad(buf, cur_x, cur_y, pending_cx, pending_cy, start_x, start_y);
    } else if (cur_x != start_x || cur_y != start_y) {
        accum_line(buf, cur_x, cur_y, start_x, start_y);
    }
}

// Scratch accumulation buffer, sized for the largest cell this project
// will rasterize into. `static` (not a local): sizeof would otherwise
// be a large stack allocation, and this project has a documented
// history of stack-overflow crashes from exactly that pattern.
#define TTF_RASTER_MAX_CELL_DIM 128
static float g_accum_storage[TTF_RASTER_MAX_CELL_DIM * TTF_RASTER_MAX_CELL_DIM];

void ttf_rasterize_glyph(
    const TTFGlyphOutline* outline,
    float scale,
    float origin_x,
    float origin_y,
    uint8_t* coverage,
    uint32_t width,
    uint32_t height
) {
    if (!outline || !coverage || width == 0 || height == 0) return;
    if (width > TTF_RASTER_MAX_CELL_DIM || height > TTF_RASTER_MAX_CELL_DIM) return;
    if (outline->contour_count == 0) return;

    for (uint32_t i = 0; i < width * height; i++) g_accum_storage[i] = 0.0f;

    AccumBuffer buf = { g_accum_storage, width, height };

    uint16_t start = 0;
    for (uint16_t c = 0; c < outline->contour_count; c++) {
        rasterize_contour(&buf, outline, start, outline->contour_end[c], scale, origin_x, origin_y);
        start = (uint16_t)(outline->contour_end[c] + 1);
    }

    for (uint32_t y = 0; y < height; y++) {
        float* row = g_accum_storage + y * width;
        uint8_t* out_row = coverage + y * width;
        float running = 0.0f;

        for (uint32_t x = 0; x < width; x++) {
            running += row[x];
            float c = running;
            if (c < 0.0f) c = -c;
            if (c > 1.0f) c = 1.0f;
            out_row[x] = (uint8_t)(c * 255.0f + 0.5f);
        }
    }
}
