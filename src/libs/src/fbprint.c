#include "drivers/video/fbprint.h"
#include "multiboot2.h"

#include "bool.h"
#include "krnl.h"

#include "mem/mem.h"
#include "drivers/files/os/ttf.h"
#include "drivers/files/os/ttf_rasterizer.h"
#include "drivers/storage/iso9660.h"

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

/*
 * ============================================================
 * FRAMEBUFFER
 * ============================================================
 */

struct Framebuffer {
    uint8_t* address;

    uint32_t pitch;
    uint32_t width;
    uint32_t height;

    uint8_t bpp;

    uint8_t red_position;
    uint8_t red_mask;

    uint8_t green_position;
    uint8_t green_mask;

    uint8_t blue_position;
    uint8_t blue_mask;
};

static struct Framebuffer framebuffer;


/*
 * ============================================================
 * FONT
 * ============================================================
 */

#define TTF_POINT_SIZE 24.0f

// Upper bound on a single glyph's rasterized cell, in pixels. Must not
// exceed TTF_RASTER_MAX_CELL_DIM in ttf_rasterizer.c (128). Generous
// enough for TTF_POINT_SIZE above with real fonts' typical bounding
// boxes (see ttf.c's TTFGlyphOutline bbox fields).
#define GLYPH_CELL_MAX 64

// CHAR_HEIGHT is fixed per line (vertical layout stays a grid; only
// horizontal glyph advance is variable-width -- see x_for_col() below).
// Set once in fbprint_init() from the loaded font's own metrics; there
// is no compile-time correct value once the font is data, not a
// baked-in bitmap array.
static uint32_t CHAR_HEIGHT = 32;

// A conservative (small) assumed minimum glyph advance, in pixels, used
// only to size text_buffer's column capacity and clamp cursor bounds
// against the framebuffer width. Real advances vary per glyph -- see
// x_for_col() -- this is not used for actual layout, only as a safe
// upper bound on "how many columns could possibly fit".
#define MIN_ASSUMED_ADVANCE 6


/*
 * ============================================================
 * LOGICAL CURSOR
 * ============================================================
 *
 * Unlike VGA text mode, the framebuffer has no character cells.
 *
 * These are therefore maintained separately:
 *
 *     cursor_col = character column
 *     cursor_row = character row
 *
 * cursor_x/y are the actual framebuffer pixel coordinates.
 */

static uint32_t cursor_x = 0;
static uint32_t cursor_y = 0;

static uint32_t cursor_col = 0;
static uint32_t cursor_row = 0;


/*
 * Maximum logical text buffer size.
 *
 * This is only used to remember characters for cursor movement
 * and deletion.
 */
#define FB_MAX_COLS 256
#define FB_MAX_ROWS 128

static char text_buffer[FB_MAX_ROWS][FB_MAX_COLS];


/*
 * ============================================================
 * COLORS
 * ============================================================
 */

static uint8_t foreground_color = PRINT_COLOR_WHITE;
static uint8_t background_color = PRINT_COLOR_BLACK;


struct RGBColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};


static const struct RGBColor color_palette[16] = {
    {   0,   0,   0 },   /* BLACK */
    {   0,   0, 170 },   /* BLUE */
    {   0, 170,   0 },   /* GREEN */
    {   0, 170, 170 },   /* CYAN */
    { 170,   0,   0 },   /* RED */
    { 170,   0, 170 },   /* MAGENTA */
    { 170,  85,   0 },   /* BROWN */
    { 170, 170, 170 },   /* LIGHT GRAY */
    {  85,  85,  85 },   /* DARK GRAY */
    {  85,  85, 255 },   /* LIGHT BLUE */
    {  85, 255,  85 },   /* LIGHT GREEN */
    {  85, 255, 255 },   /* LIGHT CYAN */
    { 255,  85,  85 },   /* LIGHT RED */
    { 255,  85, 255 },   /* PINK */
    { 255, 255,  85 },   /* YELLOW */
    { 255, 255, 255 }    /* WHITE */
};


void fbprint_set_color(uint8_t foreground, uint8_t background)
{
    if (foreground > 15)
        foreground = PRINT_COLOR_WHITE;

    if (background > 15)
        background = PRINT_COLOR_BLACK;

    foreground_color = foreground;
    background_color = background;
}


/*
 * ============================================================
 * FONT GLYPHS
 * ============================================================
 */

// The loaded TTF font. font_loaded guards every use of `font` below --
// fbprint_init() must succeed before any drawing happens.
static TTFFont font;
static bool font_loaded = false;
static float glyph_scale = 0.0f; // pixels per font unit, at TTF_POINT_SIZE
static float glyph_baseline = 0.0f; // pixel y-offset from a cell's top to the baseline

// Per-glyph metrics needed by callers that only need layout info (not
// pixels) -- used for cursor advance and wrap decisions without paying
// for a full rasterize.
static uint32_t glyph_advance_width(uint8_t c)
{
    if (!font_loaded)
        return MIN_ASSUMED_ADVANCE;

    uint16_t gid = ttf_char_to_glyph(&font, (uint32_t)c);

    // static: TTFGlyphOutline is ~9.4KB, far too large for a stack
    // local -- see ttf.h's warning on this exact point. This function
    // is never called recursively/reentrantly (no interrupt handler
    // calls into fbprint), so a single static instance is safe here,
    // same reasoning as ttf_rasterizer.c's g_accum_storage.
    static TTFGlyphOutline outline;
    if (!ttf_get_glyph_outline(&font, gid, &outline))
        return MIN_ASSUMED_ADVANCE;

    uint32_t advance = (uint32_t)(outline.advance_width * glyph_scale + 0.5f);
    return advance > 0 ? advance : MIN_ASSUMED_ADVANCE;
}


/*
 * ============================================================
 * PIXEL FUNCTIONS
 * ============================================================
 */

static uint32_t make_pixel(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t pixel = 0;

    pixel |= ((uint32_t)r &
              ((1u << framebuffer.red_mask) - 1))
             << framebuffer.red_position;

    pixel |= ((uint32_t)g &
              ((1u << framebuffer.green_mask) - 1))
             << framebuffer.green_position;

    pixel |= ((uint32_t)b &
              ((1u << framebuffer.blue_mask) - 1))
             << framebuffer.blue_position;

    return pixel;
}


void fb_put_pixel(
    uint32_t x,
    uint32_t y,
    uint8_t r,
    uint8_t g,
    uint8_t b
)
{
    if (x >= framebuffer.width ||
        y >= framebuffer.height)
        return;

    if (framebuffer.bpp != 32)
        return;

    uint32_t pixel = make_pixel(r, g, b);

    uint32_t* row =
        (uint32_t*)(
            framebuffer.address +
            (uint64_t)y * framebuffer.pitch
        );

    row[x] = pixel;
}


/*
 * ============================================================
 * INITIALIZATION
 * ============================================================
 */

int fbprint_init(uint64_t multiboot_info_addr)
{
    const char* fontPath = "/fonts/jbm.ttf";
    uint32_t fontFileSize = iso9660_get_file_size(fontPath);

    // NOTE: this buffer is intentionally never freed. TTFFont::data
    // (set by validate_ttf() below) points directly into these bytes,
    // and every future glyph lookup (ttf_char_to_glyph/
    // ttf_get_glyph_outline) dereferences through it -- unlike the old
    // LFH bitmap font, which copied its (much smaller) glyph data into
    // a static array and could free its source buffer immediately.
    uint8_t* fontBuffer = (uint8_t*)kmalloc(fontFileSize);
    uint32_t bytesRead;

    if (!iso9660_read_file(fontPath, fontBuffer, &bytesRead)) {
        KERNEL_PANIC(__FILE_NAME__, __FUNCTION__, __LINE__, "FAILED TO READ TTF FILE", 1);
    }

    if (!validate_ttf(fontBuffer, fontFileSize, &font)) {
        KERNEL_PANIC(__FILE_NAME__, __FUNCTION__, __LINE__, "INVALID TTF FILE", 1);
    }

    font_loaded = true;
    glyph_scale = TTF_POINT_SIZE / (float)font.units_per_em;

    // Baseline placement within a CHAR_HEIGHT-tall cell: leave a small
    // margin above the ascent and below the descent so glyphs (and
    // accents/descenders) don't touch adjacent rows. hhea's ascender/
    // descender aren't parsed by ttf.c (not needed for glyph outlines
    // themselves), so derive a reasonable line height directly from
    // the requested point size instead.
    CHAR_HEIGHT = (uint32_t)(TTF_POINT_SIZE * 1.3f + 0.5f);
    glyph_baseline = TTF_POINT_SIZE * 1.05f;

    uint8_t* base =
        (uint8_t*)(uintptr_t)multiboot_info_addr;

    uint32_t total_size = *(uint32_t*)base;


    uint8_t* tag_ptr = base + 8;
    uint8_t* end = base + total_size;

    while (tag_ptr < end) {

        struct multiboot_tag* tag =
            (struct multiboot_tag*)tag_ptr;

        if (tag->type == MULTIBOOT_TAG_TYPE_END)
            break;

        if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER) {

            struct multiboot_tag_framebuffer_common* fb =
                (struct multiboot_tag_framebuffer_common*)tag;

            if (fb->framebuffer_type !=
                MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {

                return 0;
            }

            framebuffer.address =
                (uint8_t*)(uintptr_t)fb->framebuffer_addr;

            framebuffer.pitch =
                fb->framebuffer_pitch;

            framebuffer.width =
                fb->framebuffer_width;

            framebuffer.height =
                fb->framebuffer_height;

            framebuffer.bpp =
                fb->framebuffer_bpp;

            struct multiboot_tag_framebuffer_rgb* rgb =
                (struct multiboot_tag_framebuffer_rgb*)tag;

            framebuffer.red_position =
                rgb->framebuffer_red_field_position;

            framebuffer.red_mask =
                rgb->framebuffer_red_mask_size;

            framebuffer.green_position =
                rgb->framebuffer_green_field_position;

            framebuffer.green_mask =
                rgb->framebuffer_green_mask_size;

            framebuffer.blue_position =
                rgb->framebuffer_blue_field_position;

            framebuffer.blue_mask =
                rgb->framebuffer_blue_mask_size;

            cursor_x = 0;
            cursor_y = 0;

            cursor_col = 0;
            cursor_row = 0;

            fbclear();

            return 1;
        }

        tag_ptr += (tag->size + 7) & ~7u;
    }

    return 0;
}


/*
 * ============================================================
 * CLEAR
 * ============================================================
 */

void fbclear(void)
{
    struct RGBColor bg =
        color_palette[background_color];

    for (uint32_t y = 0;
         y < framebuffer.height;
         y++) {

        for (uint32_t x = 0;
             x < framebuffer.width;
             x++) {

            fb_put_pixel(
                x,
                y,
                bg.r,
                bg.g,
                bg.b
            );
        }
    }

    for (uint32_t row = 0;
         row < FB_MAX_ROWS;
         row++) {

        for (uint32_t col = 0;
             col < FB_MAX_COLS;
             col++) {

            text_buffer[row][col] = ' ';
        }
    }

    cursor_x = 0;
    cursor_y = 0;

    cursor_col = 0;
    cursor_row = 0;
}


/*
 * ============================================================
 * GLYPH DRAWING
 * ============================================================
 */

static uint32_t draw_glyph(
    uint8_t c,
    uint32_t x,
    uint32_t y
)
{
    struct RGBColor fg =
        color_palette[foreground_color];

    struct RGBColor bg =
        color_palette[background_color];

    uint32_t advance = glyph_advance_width(c);

    /*
     * Clear the full cell first (background), including the space
     * between glyph and next cursor position. Unlike the old
     * fixed-width bitmap font, a glyph's own ink can be narrower or
     * wider than its advance, and the previous character drawn at this
     * same cell might have had a different (e.g. wider) advance -- so
     * clear the whole line height x this glyph's advance width, not
     * just the glyph's own bounding box.
     */
    for (uint32_t cy = y; cy < y + CHAR_HEIGHT; cy++) {
        for (uint32_t cx = x; cx < x + advance; cx++) {
            fb_put_pixel(cx, cy, bg.r, bg.g, bg.b);
        }
    }

    if (!font_loaded || c == ' ' || c == '\0')
        return advance;

    uint16_t gid = ttf_char_to_glyph(&font, (uint32_t)c);

    // static: see glyph_advance_width()'s identical comment -- both
    // TTFGlyphOutline (~9.4KB) and the coverage buffer are far too
    // large for stack locals, and this function is never reentrant.
    static TTFGlyphOutline outline;
    static uint8_t coverage[GLYPH_CELL_MAX * GLYPH_CELL_MAX];

    if (!ttf_get_glyph_outline(&font, gid, &outline))
        return advance;

    if (outline.contour_count == 0)
        return advance; // e.g. space, or a glyph with no visible ink

    for (uint32_t i = 0; i < GLYPH_CELL_MAX * GLYPH_CELL_MAX; i++)
        coverage[i] = 0;

    ttf_rasterize_glyph(
        &outline,
        glyph_scale,
        0.0f,
        glyph_baseline,
        coverage,
        GLYPH_CELL_MAX,
        GLYPH_CELL_MAX
    );

    for (uint32_t row = 0; row < GLYPH_CELL_MAX && row < CHAR_HEIGHT; row++) {
        for (uint32_t col = 0; col < GLYPH_CELL_MAX; col++) {
            uint8_t cov = coverage[row * GLYPH_CELL_MAX + col];
            if (cov == 0)
                continue; // fully transparent -- background already drawn above

            uint8_t r, g, b;
            if (cov >= 255) {
                r = fg.r; g = fg.g; b = fg.b;
            } else {
                // Linear alpha blend: this is what actually produces
                // anti-aliasing on screen -- partial coverage pixels
                // become a mix of foreground and background, not a
                // binary on/off choice.
                r = (uint8_t)(((uint32_t)fg.r * cov + (uint32_t)bg.r * (255 - cov)) / 255);
                g = (uint8_t)(((uint32_t)fg.g * cov + (uint32_t)bg.g * (255 - cov)) / 255);
                b = (uint8_t)(((uint32_t)fg.b * cov + (uint32_t)bg.b * (255 - cov)) / 255);
            }

            fb_put_pixel(x + col, y + row, r, g, b);
        }
    }

    return advance;
}


/*
 * ============================================================
 * SCROLLING
 * ============================================================
 */

static void fb_newline(void)
{
    cursor_col = 0;
    cursor_x = 0;

    cursor_row++;
    cursor_y += CHAR_HEIGHT;

    /*
     * Scroll once the cursor would move past the last row that
     * actually fits on screen at the current resolution -- not once
     * it hits the logical text-buffer's storage capacity (FB_MAX_ROWS,
     * which is just an upper bound on how much history we can track
     * for backspace/cursor movement). FB_MAX_ROWS is typically much
     * larger than what's visible, so comparing against it meant lines
     * kept getting drawn below the bottom of the framebuffer (silently
     * discarded by fb_put_pixel's bounds check) long before a scroll
     * was ever triggered.
     */
    uint32_t visible_rows = framebuffer.height / CHAR_HEIGHT;

    if (visible_rows == 0)
        visible_rows = 1;

    if (visible_rows > FB_MAX_ROWS)
        visible_rows = FB_MAX_ROWS;

    if (cursor_row < visible_rows)
        return;

    /*
     * Scroll the logical character buffer, within the visible window.
     */
    for (uint32_t row = 1;
         row < visible_rows;
         row++) {

        for (uint32_t col = 0;
             col < FB_MAX_COLS;
             col++) {

            text_buffer[row - 1][col] =
                text_buffer[row][col];
        }
    }

    for (uint32_t col = 0;
         col < FB_MAX_COLS;
         col++) {

        text_buffer[visible_rows - 1][col] = ' ';
    }

    cursor_row = visible_rows - 1;

    /*
     * Scroll framebuffer upward by one character row.
     */
    uint32_t scroll = CHAR_HEIGHT;

    if (scroll >= framebuffer.height) {
        fbclear();
        cursor_col = 0;
        cursor_row = 0;
        cursor_x = 0;
        cursor_y = 0;
        return;
    }

    for (uint32_t y = scroll;
         y < framebuffer.height;
         y++) {

        uint8_t* dst =
            framebuffer.address +
            (uint64_t)(y - scroll) *
            framebuffer.pitch;

        uint8_t* src =
            framebuffer.address +
            (uint64_t)y *
            framebuffer.pitch;

        for (uint32_t x = 0;
             x < framebuffer.pitch;
             x++) {

            dst[x] = src[x];
        }
    }

    struct RGBColor bg =
        color_palette[background_color];

    for (uint32_t y =
             framebuffer.height - scroll;
         y < framebuffer.height;
         y++) {

        for (uint32_t x = 0;
             x < framebuffer.width;
             x++) {

            fb_put_pixel(
                x,
                y,
                bg.r,
                bg.g,
                bg.b
            );
        }
    }

    cursor_y =
        framebuffer.height - CHAR_HEIGHT;
}


/*
 * ============================================================
 * COLUMN -> PIXEL X (variable-width glyph advance)
 * ============================================================
 *
 * With a fixed-width bitmap font, a column's pixel x was simply
 * col * CHAR_WIDTH. TTF glyphs have per-character advance widths, so a
 * row/col pair's actual pixel x is the sum of every character's
 * advance width before it on that row -- reconstructed from
 * text_buffer, the same logical record cursor movement/backspace
 * already relied on for "what character is at this cell".
 */

static uint32_t x_for_col(uint32_t row, uint32_t col)
{
    if (row >= FB_MAX_ROWS)
        return 0;

    if (col > FB_MAX_COLS)
        col = FB_MAX_COLS;

    uint32_t x = 0;
    for (uint32_t i = 0; i < col; i++) {
        x += glyph_advance_width((uint8_t)text_buffer[row][i]);
    }
    return x;
}


/*
 * ============================================================
 * CHARACTER OUTPUT
 * ============================================================
 */

void fbprintc(char character)
{
    if (character == '\0')
        return;


    /*
     * Newline.
     */
    if (character == '\n') {

        if (cursor_row < FB_MAX_ROWS) {

            /*
             * Mark remaining cells as empty.
             */
            for (uint32_t col = cursor_col;
                 col < FB_MAX_COLS;
                 col++) {

                text_buffer[cursor_row][col] = ' ';
            }
        }

        fb_newline();
        return;
    }


    /*
     * Carriage return.
     */
    if (character == '\r') {

        cursor_col = 0;
        cursor_x = 0;

        return;
    }


    /*
     * Backspace.
     */
    if (character == '\b') {

        fbdelete_last_char();
        return;
    }


    /*
     * Wrap at right edge. Uses this specific glyph's real advance
     * width, not a fixed CHAR_WIDTH -- a wide glyph might not fit even
     * where a narrow one would have.
     */
    if (cursor_x + glyph_advance_width((uint8_t)character) >
        framebuffer.width) {

        fb_newline();
    }


    /*
     * If we've run out of logical rows,
     * scroll first.
     */
    if (cursor_row >= FB_MAX_ROWS) {
        fb_newline();
    }


    uint32_t advance = draw_glyph(
        character,
        cursor_x,
        cursor_y
    );


    /*
     * Remember character.
     */
    if (cursor_row < FB_MAX_ROWS &&
        cursor_col < FB_MAX_COLS) {

        text_buffer[cursor_row][cursor_col] =
            character;
    }


    cursor_col++;
    cursor_x += advance;
}


/*
 * ============================================================
 * STRING OUTPUT
 * ============================================================
 */

void fbprint(const char* str)
{
    if (str == NULL)
        return;

    for (size_t i = 0;
         str[i] != '\0';
         i++) {

        fbprintc(str[i]);
    }
}


void fbprint_constant(const char* str)
{
    if (str == NULL)
        return;

    for (size_t i = 0;
         str[i] != '\0';
         i++) {

        fbprintc(str[i]);
    }
}


void fbprintln(const char* str)
{
    fbprint(str);
    fbprintc('\n');
}

void fbprint_eachChar() {
#if defined(TESTING)
    fbprintln("!\"#$%&'()*+,-./01234567");
    fbprintln("89:;<=>?@ABCDEFGHIJKLMNO");
    fbprintln("PQRSTUVWXYZ[\\]^_`abcdefg");
    fbprintln("hijklmnopqrstuvwxyz{|}~€");
    fbprintln("‚ƒ„…†‡ˆ‰Š‹ŒŽ‘’“”•–—˜™š›œ");
    fbprintln("žŸ ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ");
    fbprintln("¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍ");
    fbprintln("ÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäå");
    fbprintln("æçèéêëìíîïðñòóôõö÷øùúûüý");
    fbprintln("þÿ");
#else
    fbprintln("This command is only available in TESTING mode");
#endif
}


/*
 * ============================================================
 * INTEGER OUTPUT
 * ============================================================
 */

void fbprint_uint64_dec(uint64_t value)
{
    if (value == 0) {
        fbprintc('0');
        return;
    }

    char buffer[20];
    int i = 0;

    while (value > 0) {

        buffer[i++] =
            (char)((value % 10) + '0');

        value /= 10;
    }

    while (i > 0) {
        i--;
        fbprintc(buffer[i]);
    }
}


void fbprint_uint64_hex(uint64_t value)
{
    if (value == 0) {
        fbprintc('0');
        return;
    }

    char buffer[16];
    int i = 0;

    while (value > 0) {

        uint8_t digit =
            (uint8_t)(value & 0xF);

        if (digit < 10)
            buffer[i++] =
                (char)('0' + digit);
        else
            buffer[i++] =
                (char)('A' + digit - 10);

        value >>= 4;
    }

    while (i > 0) {
        i--;
        fbprintc(buffer[i]);
    }
}


void fbprint_uint64_bin(uint64_t value)
{
    char buffer[64];

    for (size_t i = 0;
         i < 64;
         i++) {

        buffer[i] =
            (char)((value & 1) + '0');

        value >>= 1;
    }

    for (size_t i = 64;
         i > 0;
         i--) {

        fbprintc(buffer[i - 1]);
    }
}


/*
 * ============================================================
 * DELETE LAST CHARACTER
 * ============================================================
 */

void fbdelete_last_char(void)
{
    if (cursor_row == 0 &&
        cursor_col == 0) {

        return;
    }


    /*
     * Move to previous character.
     */
    if (cursor_col > 0) {

        cursor_col--;

    } else {

        /*
         * Move to previous line.
         */
        if (cursor_row == 0)
            return;

        cursor_row--;

        /*
         * Find last character on previous row.
         */
        cursor_col = 0;

        for (uint32_t col = 0;
             col < FB_MAX_COLS;
             col++) {

            if (text_buffer[cursor_row][col] != ' ')
                cursor_col = col + 1;
        }

        if (cursor_col > 0)
            cursor_col--;
    }


    cursor_x =
        x_for_col(cursor_row, cursor_col);

    cursor_y =
        cursor_row * CHAR_HEIGHT;


    /*
     * Erase the character. Uses the actual character still recorded at
     * this cell's real advance width (about to be overwritten with a
     * space below), not a fixed CHAR_WIDTH.
     */
    uint32_t erase_width =
        (cursor_row < FB_MAX_ROWS && cursor_col < FB_MAX_COLS)
            ? glyph_advance_width((uint8_t)text_buffer[cursor_row][cursor_col])
            : MIN_ASSUMED_ADVANCE;

    struct RGBColor bg =
        color_palette[background_color];

    for (uint32_t y = cursor_y;
         y < cursor_y + CHAR_HEIGHT &&
         y < framebuffer.height;
         y++) {

        for (uint32_t x = cursor_x;
             x < cursor_x + erase_width &&
             x < framebuffer.width;
             x++) {

            fb_put_pixel(
                x,
                y,
                bg.r,
                bg.g,
                bg.b
            );
        }
    }


    if (cursor_row < FB_MAX_ROWS &&
        cursor_col < FB_MAX_COLS) {

        text_buffer[cursor_row][cursor_col] =
            ' ';
    }
}


/*
 * ============================================================
 * CURSOR
 * ============================================================
 */

void fbmove_cursor(int row, int col)
{
    if (row < 0)
        row = 0;

    if (col < 0)
        col = 0;


    uint32_t max_rows =
        framebuffer.height / CHAR_HEIGHT;

    // Conservative (upper-bound) column count, since advance width
    // varies per glyph -- used only to keep `col` from being clamped
    // to something absurd, not for actual layout.
    uint32_t max_cols =
        framebuffer.width / MIN_ASSUMED_ADVANCE;

    if (max_cols > FB_MAX_COLS)
        max_cols = FB_MAX_COLS;


    if (max_rows == 0 ||
        max_cols == 0) {

        return;
    }


    if ((uint32_t)row >= max_rows)
        row = (int)max_rows - 1;

    if ((uint32_t)col >= max_cols)
        col = (int)max_cols - 1;


    cursor_row = (uint32_t)row;
    cursor_col = (uint32_t)col;

    cursor_x =
        x_for_col(cursor_row, cursor_col);

    cursor_y =
        cursor_row * CHAR_HEIGHT;
}


void fbmove_cursor_up(void)
{
    if (cursor_row == 0)
        return;

    cursor_row--;

    cursor_y =
        cursor_row * CHAR_HEIGHT;
}


void fbmove_cursor_down(void)
{
    uint32_t max_rows =
        framebuffer.height / CHAR_HEIGHT;

    if (max_rows == 0)
        return;

    if (cursor_row + 1 >= max_rows)
        return;

    cursor_row++;

    cursor_y =
        cursor_row * CHAR_HEIGHT;
}


void fbmove_cursor_left(void)
{
    if (cursor_col > 0) {

        cursor_col--;

    } else if (cursor_row > 0) {

        cursor_row--;

        /*
         * Find last character on previous row.
         */
        cursor_col = 0;

        for (uint32_t col = 0;
             col < FB_MAX_COLS;
             col++) {

            if (text_buffer[cursor_row][col] != ' ')
                cursor_col = col + 1;
        }

        if (cursor_col > 0)
            cursor_col--;
    }

    cursor_x =
        x_for_col(cursor_row, cursor_col);

    cursor_y =
        cursor_row * CHAR_HEIGHT;
}


void fbmove_cursor_right(void)
{
    uint32_t max_cols =
        framebuffer.width / MIN_ASSUMED_ADVANCE;

    if (max_cols > FB_MAX_COLS)
        max_cols = FB_MAX_COLS;

    uint32_t max_rows =
        framebuffer.height / CHAR_HEIGHT;

    if (max_cols == 0 ||
        max_rows == 0) {

        return;
    }


    /*
     * Don't move beyond the logical line.
     */
    uint32_t last_col = 0;

    if (cursor_row < FB_MAX_ROWS) {

        for (uint32_t col = 0;
             col < FB_MAX_COLS;
             col++) {

            if (text_buffer[cursor_row][col] != ' ')
                last_col = col + 1;
        }
    }


    if (cursor_col < last_col) {

        cursor_col++;

    } else if (cursor_col + 1 < max_cols) {

        cursor_col++;

    } else if (cursor_row + 1 < max_rows) {

        cursor_row++;
        cursor_col = 0;
    }


    cursor_x =
        x_for_col(cursor_row, cursor_col);

    cursor_y =
        cursor_row * CHAR_HEIGHT;
}


void fbmove_cursor_to_start(void)
{
    cursor_row = 0;
    cursor_col = 0;

    cursor_x = 0;
    cursor_y = 0;
}


/*
 * ============================================================
 * PRINTF
 * ============================================================
 */

static void fbprint_int64_dec(int64_t value)
{
    if (value < 0) {

        fbprintc('-');

        /*
         * Avoid overflow when value == INT64_MIN.
         */
        uint64_t magnitude =
            (uint64_t)(-(value + 1)) + 1;

        fbprint_uint64_dec(magnitude);
        return;
    }

    fbprint_uint64_dec((uint64_t)value);
}


void fbprintf(const char* fmt, ...)
{
    if (fmt == NULL)
        return;

    va_list args;

    va_start(args, fmt);


    for (size_t i = 0;
         fmt[i] != '\0';
         i++) {

        if (fmt[i] != '%') {

            fbprintc(fmt[i]);
            continue;
        }


        /*
         * Get format character.
         */
        i++;

        if (fmt[i] == '\0')
            break;


        switch (fmt[i]) {

            case '%':
                fbprintc('%');
                break;


            case 'c': {
                int c = va_arg(args, int);
                fbprintc((char)c);
                break;
            }


            case 's': {
                const char* str =
                    va_arg(args, const char*);

                fbprint(str);
                break;
            }


            case 'd': {
                int64_t value =
                    va_arg(args, int64_t);

                fbprint_int64_dec(value);
                break;
            }


            case 'u': {
                uint64_t value =
                    va_arg(args, uint64_t);

                fbprint_uint64_dec(value);
                break;
            }


            case 'x':
            case 'X': {
                uint64_t value =
                    va_arg(args, uint64_t);

                fbprint_uint64_hex(value);
                break;
            }


            case 'b': {
                uint64_t value =
                    va_arg(args, uint64_t);

                fbprint_uint64_bin(value);
                break;
            }


            default:

                /*
                 * Unknown format:
                 * print it literally.
                 */
                fbprintc('%');
                fbprintc(fmt[i]);
                break;
        }
    }


    va_end(args);
}