#include "drivers/files/os/ttf.h"

#include <stddef.h>
#include "debug.h"

/* ------------------------------------------------------------------ */
/* Big-endian reads -- every multi-byte field in an sfnt file is BE     */
/* ------------------------------------------------------------------ */

static uint16_t read_u16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}
static int16_t read_i16(const uint8_t* p) {
    return (int16_t)read_u16(p);
}
static uint32_t read_u32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

/* ------------------------------------------------------------------ */
/* Table directory                                                      */
/* ------------------------------------------------------------------ */

#define TAG(a,b,c,d) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(c)<<8)|(uint32_t)(d))

static bool find_table(const uint8_t* data, uint32_t size, uint32_t tag, uint32_t* out_offset, uint32_t* out_length) {
    if (size < 12) return false;

    uint32_t sfnt_version = read_u32(data);
    if (sfnt_version != 0x00010000 &&
        sfnt_version != TAG('t','r','u','e') &&
        sfnt_version != TAG('t','y','p','1')) {
        return false;
    }

    uint16_t num_tables = read_u16(data + 4);
    uint32_t record_base = 12;

    if ((uint64_t)record_base + (uint64_t)num_tables * 16 > size) return false;

    for (uint16_t i = 0; i < num_tables; i++) {
        const uint8_t* rec = data + record_base + (uint32_t)i * 16;
        uint32_t rec_tag = read_u32(rec);

        if (rec_tag == tag) {
            uint32_t offset = read_u32(rec + 8);
            uint32_t length = read_u32(rec + 12);

            if ((uint64_t)offset + (uint64_t)length > size) return false;

            *out_offset = offset;
            *out_length = length;
            return true;
        }
    }

    return false;
}

/* ------------------------------------------------------------------ */
/* head / maxp / hhea                                                   */
/* ------------------------------------------------------------------ */

bool validate_ttf(const uint8_t* data, uint32_t size, TTFFont* font) {
    if (!data || !font || size < 12) {
        DBG_PRINTLNS("validate_ttf: FAIL null/too-small");
        return false;
    }

    font->data = data;
    font->size = size;
    font->loca_offset = 0;
    font->loca_length = 0;
    font->glyf_offset = 0;
    font->glyf_length = 0;
    font->hhea_offset = 0;
    font->hmtx_offset = 0;
    font->cmap_offset = 0;
    font->cmap_subtable_offset = 0;
    font->cmap_format = 0;

    uint32_t head_len, maxp_len;
    if (!find_table(data, size, TAG('h','e','a','d'), &font->head_offset, &head_len)) {
        DBG_PRINTLNS("validate_ttf: FAIL no head table (bad sfnt version / table directory?)");
        return false;
    }
    if (head_len < 54) {
        DBG_PRINTLNS("validate_ttf: FAIL head table too short");
        return false;
    }

    if (!find_table(data, size, TAG('m','a','x','p'), &font->maxp_offset, &maxp_len)) {
        DBG_PRINTLNS("validate_ttf: FAIL no maxp table");
        return false;
    }
    if (maxp_len < 6) {
        DBG_PRINTLNS("validate_ttf: FAIL maxp table too short");
        return false;
    }

    if (!find_table(data, size, TAG('l','o','c','a'), &font->loca_offset, &font->loca_length)) {
        DBG_PRINTLNS("validate_ttf: FAIL no loca table (CFF/OTTO font? not supported)");
        return false;
    }
    if (!find_table(data, size, TAG('g','l','y','f'), &font->glyf_offset, &font->glyf_length)) {
        DBG_PRINTLNS("validate_ttf: FAIL no glyf table (CFF/OTTO font? not supported)");
        return false;
    }

    {
        uint32_t cmap_len;
        if (!find_table(data, size, TAG('c','m','a','p'), &font->cmap_offset, &cmap_len)) {
            DBG_PRINTLNS("validate_ttf: FAIL no cmap table");
            return false;
        }
    }

    const uint8_t* head = data + font->head_offset;
    font->units_per_em = read_u16(head + 18);
    if (font->units_per_em == 0) {
        DBG_PRINTLNS("validate_ttf: FAIL units_per_em == 0");
        return false;
    }

    int16_t index_to_loc_format = read_i16(head + 50);
    if (index_to_loc_format != 0 && index_to_loc_format != 1) {
        DBG_PRINTLNS("validate_ttf: FAIL indexToLocFormat not 0 or 1");
        return false;
    }
    font->loca_is_long = (index_to_loc_format == 1);

    const uint8_t* maxp = data + font->maxp_offset;
    font->num_glyphs = read_u16(maxp + 4);
    if (font->num_glyphs == 0) {
        DBG_PRINTLNS("validate_ttf: FAIL num_glyphs == 0");
        return false;
    }

    uint32_t expected_loca_len = font->loca_is_long
        ? (uint32_t)(font->num_glyphs + 1) * 4
        : (uint32_t)(font->num_glyphs + 1) * 2;
    if (font->loca_length < expected_loca_len) {
        DBG_PRINTLNS("validate_ttf: FAIL loca table shorter than num_glyphs+1 requires");
        return false;
    }

    uint32_t hhea_len;
    if (find_table(data, size, TAG('h','h','e','a'), &font->hhea_offset, &hhea_len) && hhea_len >= 36) {
        uint32_t hmtx_len;
        find_table(data, size, TAG('h','m','t','x'), &font->hmtx_offset, &hmtx_len);
    }

    const uint8_t* cmap = data + font->cmap_offset;
    uint32_t cmap_table_size = size - font->cmap_offset;
    if (cmap_table_size < 4) {
        DBG_PRINTLNS("validate_ttf: FAIL cmap table region too small");
        return false;
    }

    uint16_t num_cmap_tables = read_u16(cmap + 2);
    if ((uint64_t)4 + (uint64_t)num_cmap_tables * 8 > cmap_table_size) {
        DBG_PRINTLNS("validate_ttf: FAIL cmap encoding record array doesn't fit in file");
        return false;
    }

    uint32_t best_offset = 0;
    uint8_t best_format = 0;
    int best_score = -1;

    for (uint16_t i = 0; i < num_cmap_tables; i++) {
        const uint8_t* rec = cmap + 4 + (uint32_t)i * 8;
        uint16_t platform_id = read_u16(rec + 0);
        uint16_t encoding_id = read_u16(rec + 2);
        uint32_t subtable_offset = read_u32(rec + 4);

        if ((uint64_t)subtable_offset + 2 > cmap_table_size) continue;
        uint16_t format = read_u16(cmap + subtable_offset);

        // DEBUG: log every cmap subtable seen, regardless of whether
        // it's usable, so we can see exactly what this font actually
        // ships (platform/encoding/format) when the "no supported
        // cmap subtable" rejection below fires.
        DBG_PRINTS("validate_ttf: cmap subtable platform=");
        DBG_PRINTLNS(platform_id == 0 ? "0(Unicode)" : platform_id == 1 ? "1(Mac)" : platform_id == 3 ? "3(Windows)" : "?(other)");

        if (format != 4 && format != 12) continue;

        int score = 0;
        if (platform_id == 3 && encoding_id == 10) score = 4;
        else if (platform_id == 0 && (encoding_id == 4 || encoding_id == 6)) score = 4;
        else if (platform_id == 3 && encoding_id == 1) score = 3;
        else if (platform_id == 0) score = 2;
        else score = 1;

        if (score > best_score) {
            best_score = score;
            best_offset = font->cmap_offset + subtable_offset;
            best_format = (uint8_t)format;
        }
    }

    if (best_score < 0) {
        DBG_PRINTLNS("validate_ttf: FAIL no cmap subtable with format 4 or 12 found");
        return false;
    }

    font->cmap_subtable_offset = best_offset;
    font->cmap_format = best_format;

    DBG_PRINTLNS("validate_ttf: OK");

    return true;
}

/* ------------------------------------------------------------------ */
/* cmap lookup                                                          */
/* ------------------------------------------------------------------ */

static uint16_t cmap_lookup_format4(const TTFFont* font, uint32_t codepoint) {
    if (codepoint > 0xFFFF) return 0;

    const uint8_t* sub = font->data + font->cmap_subtable_offset;
    uint16_t seg_x2 = read_u16(sub + 6);
    uint16_t seg_count = seg_x2 / 2;

    const uint8_t* end_codes    = sub + 14;
    const uint8_t* start_codes  = end_codes + seg_x2 + 2;
    const uint8_t* id_deltas    = start_codes + seg_x2;
    const uint8_t* id_range_off = id_deltas + seg_x2;

    for (uint16_t i = 0; i < seg_count; i++) {
        uint16_t end_code = read_u16(end_codes + i * 2);
        if (codepoint > end_code) continue;

        uint16_t start_code = read_u16(start_codes + i * 2);
        if (codepoint < start_code) return 0;

        int16_t id_delta = read_i16(id_deltas + i * 2);
        uint16_t id_range_offset = read_u16(id_range_off + i * 2);

        if (id_range_offset == 0) {
            return (uint16_t)(codepoint + id_delta);
        }

        const uint8_t* glyph_id_ptr =
            id_range_off + i * 2 + id_range_offset + (uint32_t)(codepoint - start_code) * 2;

        if (glyph_id_ptr + 2 > font->data + font->size) return 0;

        uint16_t glyph_id = read_u16(glyph_id_ptr);
        if (glyph_id == 0) return 0;
        return (uint16_t)(glyph_id + id_delta);
    }

    return 0;
}

static uint16_t cmap_lookup_format12(const TTFFont* font, uint32_t codepoint) {
    const uint8_t* sub = font->data + font->cmap_subtable_offset;
    uint32_t num_groups = read_u32(sub + 12);

    const uint8_t* groups = sub + 16;

    for (uint32_t i = 0; i < num_groups; i++) {
        const uint8_t* g = groups + i * 12;
        uint32_t start_char = read_u32(g + 0);
        uint32_t end_char   = read_u32(g + 4);
        uint32_t start_glyph = read_u32(g + 8);

        if (codepoint >= start_char && codepoint <= end_char) {
            return (uint16_t)(start_glyph + (codepoint - start_char));
        }
    }

    return 0;
}

uint16_t ttf_char_to_glyph(const TTFFont* font, uint32_t codepoint) {
    if (!font || font->cmap_subtable_offset == 0) return 0;

    if (font->cmap_format == 4) return cmap_lookup_format4(font, codepoint);
    if (font->cmap_format == 12) return cmap_lookup_format12(font, codepoint);
    return 0;
}

/* ------------------------------------------------------------------ */
/* glyf outline extraction                                              */
/* ------------------------------------------------------------------ */

static uint32_t loca_entry(const TTFFont* font, uint16_t glyph_index) {
    const uint8_t* loca = font->data + font->loca_offset;

    if (font->loca_is_long) {
        return read_u32(loca + (uint32_t)glyph_index * 4);
    } else {
        return (uint32_t)read_u16(loca + (uint32_t)glyph_index * 2) * 2;
    }
}

static uint16_t advance_width_for(const TTFFont* font, uint16_t glyph_index) {
    if (font->hhea_offset == 0 || font->hmtx_offset == 0) return 0;

    const uint8_t* hhea = font->data + font->hhea_offset;
    uint16_t num_h_metrics = read_u16(hhea + 34);
    if (num_h_metrics == 0) return 0;

    const uint8_t* hmtx = font->data + font->hmtx_offset;

    uint16_t metric_index = glyph_index < num_h_metrics ? glyph_index : (uint16_t)(num_h_metrics - 1);

    if ((uint64_t)font->hmtx_offset + (uint64_t)metric_index * 4 + 2 > font->size) return 0;

    return read_u16(hmtx + (uint32_t)metric_index * 4);
}

static bool parse_glyph(const TTFFont* font, uint16_t glyph_index, TTFGlyphOutline* out, int depth);

static bool parse_simple_glyph(const uint8_t* g, uint32_t glyph_len, int16_t num_contours, TTFGlyphOutline* out) {
    if ((uint32_t)num_contours > TTF_MAX_CONTOURS_PER_GLYPH) return false;

    const uint8_t* p = g + 10;

    if ((uint32_t)(p - g) + (uint32_t)num_contours * 2 + 2 > glyph_len) return false;

    for (int16_t i = 0; i < num_contours; i++) {
        out->contour_end[i] = read_u16(p + i * 2);
    }
    out->contour_count = (uint16_t)num_contours;

    uint16_t point_count = num_contours > 0 ? (uint16_t)(out->contour_end[num_contours - 1] + 1) : 0;
    if (point_count > TTF_MAX_POINTS_PER_GLYPH) return false;
    out->point_count = point_count;

    p += (uint32_t)num_contours * 2;

    uint16_t instruction_len = read_u16(p);
    p += 2 + instruction_len;

    if ((uint32_t)(p - g) > glyph_len) return false;

    uint8_t flags[TTF_MAX_POINTS_PER_GLYPH];
    uint16_t flag_count = 0;

    while (flag_count < point_count) {
        if ((uint32_t)(p - g) + 1 > glyph_len) return false;
        uint8_t flag = *p++;
        flags[flag_count++] = flag;

        if (flag & 0x08) {
            if ((uint32_t)(p - g) + 1 > glyph_len) return false;
            uint8_t repeat = *p++;
            while (repeat-- > 0 && flag_count < point_count) {
                flags[flag_count++] = flag;
            }
        }
    }

    int32_t x = 0;
    for (uint16_t i = 0; i < point_count; i++) {
        uint8_t flag = flags[i];
        int32_t dx = 0;

        if (flag & 0x02) {
            if ((uint32_t)(p - g) + 1 > glyph_len) return false;
            uint8_t v = *p++;
            dx = (flag & 0x10) ? v : -(int32_t)v;
        } else if (!(flag & 0x10)) {
            if ((uint32_t)(p - g) + 2 > glyph_len) return false;
            dx = read_i16(p);
            p += 2;
        }

        x += dx;
        out->points[i].x = (float)x;
        out->points[i].on_curve = (flag & 0x01) != 0;
    }

    int32_t y = 0;
    for (uint16_t i = 0; i < point_count; i++) {
        uint8_t flag = flags[i];
        int32_t dy = 0;

        if (flag & 0x04) {
            if ((uint32_t)(p - g) + 1 > glyph_len) return false;
            uint8_t v = *p++;
            dy = (flag & 0x20) ? v : -(int32_t)v;
        } else if (!(flag & 0x20)) {
            if ((uint32_t)(p - g) + 2 > glyph_len) return false;
            dy = read_i16(p);
            p += 2;
        }

        y += dy;
        out->points[i].y = (float)y;
    }

    return true;
}

static bool parse_composite_glyph(const TTFFont* font, const uint8_t* g, uint32_t glyph_len, TTFGlyphOutline* out, int depth) {
    if (depth > 4) return false;

    // Scratch storage for the component glyph being resolved at this
    // recursion level. TTFGlyphOutline is ~9.4KB -- far too large for
    // an ordinary stack local, especially in a function that recurses
    // (composite glyphs can reference other composites). A single
    // static wouldn't be safe here since nested recursive calls would
    // alias the same storage; index by depth instead, which is safe
    // since depth is bounded (checked above) and each level only needs
    // its slot for the duration of its own call, never concurrently
    // with a deeper level's use of ITS slot.
    static TTFGlyphOutline component_scratch[5];
    TTFGlyphOutline* component = &component_scratch[depth];

    out->contour_count = 0;
    out->point_count = 0;

    const uint8_t* p = g + 10;
    bool more = true;

    while (more) {
        if ((uint32_t)(p - g) + 4 > glyph_len) return false;

        uint16_t component_flags = read_u16(p);
        uint16_t component_glyph_index = read_u16(p + 2);
        p += 4;

        int32_t dx = 0, dy = 0;
        bool args_are_words = (component_flags & 0x0001) != 0;
        bool args_are_xy = (component_flags & 0x0002) != 0;

        if (args_are_words) {
            if ((uint32_t)(p - g) + 4 > glyph_len) return false;
            if (args_are_xy) { dx = read_i16(p); dy = read_i16(p + 2); }
            p += 4;
        } else {
            if ((uint32_t)(p - g) + 2 > glyph_len) return false;
            if (args_are_xy) { dx = (int8_t)p[0]; dy = (int8_t)p[1]; }
            p += 2;
        }

        float a = 1.0f, b = 0.0f, c = 0.0f, d = 1.0f;

        if (component_flags & 0x0008) {
            if ((uint32_t)(p - g) + 2 > glyph_len) return false;
            a = d = (float)read_i16(p) / 16384.0f;
            p += 2;
        } else if (component_flags & 0x0040) {
            if ((uint32_t)(p - g) + 4 > glyph_len) return false;
            a = (float)read_i16(p) / 16384.0f;
            d = (float)read_i16(p + 2) / 16384.0f;
            p += 4;
        } else if (component_flags & 0x0080) {
            if ((uint32_t)(p - g) + 8 > glyph_len) return false;
            a = (float)read_i16(p) / 16384.0f;
            b = (float)read_i16(p + 2) / 16384.0f;
            c = (float)read_i16(p + 4) / 16384.0f;
            d = (float)read_i16(p + 6) / 16384.0f;
            p += 8;
        }

        if (!parse_glyph(font, component_glyph_index, component, depth + 1)) return false;

        uint16_t point_base = out->point_count;
        if ((uint32_t)point_base + component->point_count > TTF_MAX_POINTS_PER_GLYPH) return false;

        for (uint16_t i = 0; i < component->point_count; i++) {
            float px = component->points[i].x;
            float py = component->points[i].y;

            out->points[point_base + i].x = a * px + c * py + (float)dx;
            out->points[point_base + i].y = b * px + d * py + (float)dy;
            out->points[point_base + i].on_curve = component->points[i].on_curve;
        }

        if (out->contour_count + component->contour_count > TTF_MAX_CONTOURS_PER_GLYPH) return false;
        for (uint16_t i = 0; i < component->contour_count; i++) {
            out->contour_end[out->contour_count + i] = (uint16_t)(component->contour_end[i] + point_base);
        }
        out->contour_count = (uint16_t)(out->contour_count + component->contour_count);
        out->point_count = (uint16_t)(out->point_count + component->point_count);

        more = (component_flags & 0x0020) != 0;
    }

    return true;
}

static bool parse_glyph(const TTFFont* font, uint16_t glyph_index, TTFGlyphOutline* out, int depth) {
    if (glyph_index >= font->num_glyphs) return false;

    uint32_t start = loca_entry(font, glyph_index);
    uint32_t end = loca_entry(font, (uint16_t)(glyph_index + 1));

    out->contour_count = 0;
    out->point_count = 0;
    out->x_min = out->y_min = out->x_max = out->y_max = 0;

    if (end <= start) {
        return true;
    }

    uint32_t glyph_len = end - start;
    if ((uint64_t)font->glyf_offset + (uint64_t)end > font->size) return false;
    if (glyph_len < 10) return false;

    const uint8_t* g = font->data + font->glyf_offset + start;

    int16_t num_contours = read_i16(g);
    out->x_min = read_i16(g + 2);
    out->y_min = read_i16(g + 4);
    out->x_max = read_i16(g + 6);
    out->y_max = read_i16(g + 8);

    bool ok;
    if (num_contours >= 0) {
        ok = parse_simple_glyph(g, glyph_len, num_contours, out);
    } else {
        ok = parse_composite_glyph(font, g, glyph_len, out, depth);
    }

    return ok;
}

bool ttf_get_glyph_outline(const TTFFont* font, uint16_t glyph_index, TTFGlyphOutline* out) {
    if (!font || !out) return false;

    if (!parse_glyph(font, glyph_index, out, 0)) return false;

    out->advance_width = advance_width_for(font, glyph_index);
    return true;
}
