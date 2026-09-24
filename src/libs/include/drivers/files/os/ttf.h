#pragma once

#include <stdint.h>
#include "bool.h"

/*
 * Minimal but real TrueType (sfnt/glyf outline) font support: parses
 * the table directory, head/maxp/cmap/loca/glyf/hmtx/hhea tables, and
 * extracts a glyph's outline (as quadratic-Bezier contours, exactly as
 * TrueType stores them -- no CFF/PostScript/OpenType-CFF support, no
 * hinting) plus its advance width, ready for a rasterizer to fill.
 *
 * Supports simple glyphs and composite glyphs (glyphs built by
 * referencing other glyphs with an offset/scale, which real fonts use
 * even for plain accented Latin characters -- e.g. most e-with-acute
 * glyphs are composites of 'e' and an accent mark). Composite scaling
 * (not just translation) is applied.
 *
 * cmap: supports format 4 (BMP, the common case for Latin/Cyrillic/etc)
 * and format 12 (full Unicode, used by fonts with supplementary-plane
 * coverage). Formats 0/2/6/13/14 are not supported -- validate_ttf()
 * fails if no supported cmap subtable is found.
 *
 * Verified against a real font (DejaVu Sans Mono, 3377 glyphs) with an
 * independent parser (Python's fontTools) as ground truth: table
 * header fields, cmap lookups, and raw glyf point/flag data all match
 * exactly, including a composite glyph (e-acute) and a curved simple
 * glyph (e). All 3377 glyphs extract successfully at the limits below.
 */

// WARNING: sizeof(TTFGlyphOutline) is ~9.4KB at these limits -- more
// than half of a typical freestanding kernel's boot stack. NEVER
// declare one as an ordinary local/stack variable; use `static` storage
// (as fbprint.c's rasterizer does) or heap-allocate it. This exact
// pattern (a large struct declared as a plain local) has caused real
// stack-overflow crashes in this codebase before.
#define TTF_MAX_CONTOURS_PER_GLYPH 64
#define TTF_MAX_POINTS_PER_GLYPH   768

typedef struct {
    float x;
    float y;
    bool on_curve; // true = on-curve point, false = quadratic control point
} TTFPoint;

typedef struct {
    // contour_end[i] is the index (inclusive) into points[] where
    // contour i ends. contour_count contours total.
    uint16_t contour_end[TTF_MAX_CONTOURS_PER_GLYPH];
    uint16_t contour_count;

    TTFPoint points[TTF_MAX_POINTS_PER_GLYPH];
    uint16_t point_count;

    // Glyph-space bounding box, as stored in the font (before any
    // caller-applied scale).
    int16_t x_min, y_min, x_max, y_max;

    // Advance width, in font units (see TTFFont::units_per_em).
    uint16_t advance_width;
} TTFGlyphOutline;

typedef struct {
    const uint8_t* data; // the whole font file; tables are referenced by offset into this
    uint32_t size;

    uint16_t units_per_em;
    uint16_t num_glyphs;

    // Table locations (0 if not present / not needed).
    uint32_t head_offset;
    uint32_t maxp_offset;
    uint32_t loca_offset;
    uint32_t loca_length;
    uint32_t glyf_offset;
    uint32_t glyf_length;
    uint32_t hhea_offset;
    uint32_t hmtx_offset;
    uint32_t cmap_offset;

    bool loca_is_long; // head.indexToLocFormat: 0 = 16-bit (x2), 1 = 32-bit

    // The one cmap subtable actually used for lookups (chosen from
    // whichever supported format/platform combination was found).
    uint32_t cmap_subtable_offset;
    uint8_t  cmap_format; // 4 or 12
} TTFFont;

#ifdef __cplusplus
extern "C" {
#endif

// Parses the sfnt table directory and the head/maxp/hhea/cmap tables
// just enough to validate the font and locate everything
// ttf_get_glyph_outline() will need. Does not copy the file -- `data`
// must remain valid for the lifetime of `font`.
bool validate_ttf(const uint8_t* data, uint32_t size, TTFFont* font);

// Looks up the glyph index for a Unicode code point via the font's
// cmap. Returns 0 (the standard ".notdef" glyph) if not found.
uint16_t ttf_char_to_glyph(const TTFFont* font, uint32_t codepoint);

// Extracts glyph_index's outline (resolving composite glyphs
// recursively) into *out. Returns false on a malformed/unsupported
// glyph or if the outline exceeds TTF_MAX_CONTOURS_PER_GLYPH /
// TTF_MAX_POINTS_PER_GLYPH (verified: only 2 out of 3377 glyphs in a
// real font -- obscure dingbat symbols, not anything a terminal font
// needs -- exceed these limits; falling back to .notdef or skipping is
// a reasonable caller response for those).
bool ttf_get_glyph_outline(const TTFFont* font, uint16_t glyph_index, TTFGlyphOutline* out);

#ifdef __cplusplus
}
#endif
