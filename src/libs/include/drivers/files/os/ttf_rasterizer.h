#pragma once

#include <stdint.h>
#include "drivers/files/os/ttf.h"

/*
 * Rasterizes a TTFGlyphOutline into an 8-bit coverage buffer (0 =
 * fully outside the glyph, 255 = fully inside/covered), using signed-
 * area accumulation (the same general technique used by stb_truetype
 * and FreeType's rasterizer): each edge contributes a signed
 * trapezoidal area to every pixel cell it crosses, an accumulator row
 * is swept left-to-right, and the running sum at each pixel gives its
 * exact fractional coverage -- proper anti-aliasing, not a binary
 * inside/outside test.
 *
 * Quadratic Bezier curves (TrueType's off-curve control points,
 * including the implied-on-curve-midpoint encoding between consecutive
 * off-curve points) are flattened into short line segments before
 * rasterization; segment count adapts to the curve's size in pixels.
 */

#ifdef __cplusplus
extern "C" {
#endif

// Rasterizes `outline` into `coverage` (a width*height byte buffer,
// row-major, one byte per pixel). The outline's own coordinates are
// multiplied by `scale` and offset by (origin_x, origin_y) to place it
// within the buffer -- typically origin_y should account for the
// glyph's ascent so y=0 in the outline (the baseline) lands at the
// correct row.
//
// Does NOT clear `coverage` first. width/height must each be <= 128
// (see ttf_rasterizer.c's internal scratch buffer); returns without
// drawing anything if exceeded.
void ttf_rasterize_glyph(
    const TTFGlyphOutline* outline,
    float scale,
    float origin_x,
    float origin_y,
    uint8_t* coverage,
    uint32_t width,
    uint32_t height
);

#ifdef __cplusplus
}
#endif
