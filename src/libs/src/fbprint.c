#include "fbprint.h"
#include "multiboot2.h"

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

#define FONT_WIDTH   5
#define FONT_HEIGHT  7
#define FONT_SCALE   2

#define CHAR_WIDTH  (FONT_WIDTH * FONT_SCALE + FONT_SCALE)
#define CHAR_HEIGHT (FONT_HEIGHT * FONT_SCALE + FONT_SCALE)


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

static const uint8_t* get_glyph(char c)
{
    static const uint8_t space[7] = {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00
    };

    static const uint8_t A[7] = {
        0x0E, 0x11, 0x11, 0x1F,
        0x11, 0x11, 0x11
    };

    static const uint8_t B[7] = {
        0x1E, 0x11, 0x11, 0x1E,
        0x11, 0x11, 0x1E
    };

    static const uint8_t C[7] = {
        0x0F, 0x10, 0x10, 0x10,
        0x10, 0x10, 0x0F
    };

    static const uint8_t D[7] = {
        0x1E, 0x11, 0x11, 0x11,
        0x11, 0x11, 0x1E
    };

    static const uint8_t E[7] = {
        0x1F, 0x10, 0x10, 0x1E,
        0x10, 0x10, 0x1F
    };

    static const uint8_t F[7] = {
        0x1F, 0x10, 0x10, 0x1E,
        0x10, 0x10, 0x10
    };

    static const uint8_t G[7] = {
        0x0F, 0x10, 0x10, 0x17,
        0x11, 0x11, 0x0F
    };

    static const uint8_t H[7] = {
        0x11, 0x11, 0x11, 0x1F,
        0x11, 0x11, 0x11
    };

    static const uint8_t I[7] = {
        0x1F, 0x04, 0x04, 0x04,
        0x04, 0x04, 0x1F
    };

    static const uint8_t J[7] = {
        0x01, 0x01, 0x01, 0x01,
        0x11, 0x11, 0x0E
    };

    static const uint8_t K[7] = {
        0x11, 0x12, 0x14, 0x18,
        0x14, 0x12, 0x11
    };

    static const uint8_t L[7] = {
        0x10, 0x10, 0x10, 0x10,
        0x10, 0x10, 0x1F
    };

    static const uint8_t M[7] = {
        0x11, 0x1B, 0x15, 0x15,
        0x11, 0x11, 0x11
    };

    static const uint8_t N[7] = {
        0x11, 0x19, 0x15, 0x13,
        0x11, 0x11, 0x11
    };

    static const uint8_t O[7] = {
        0x0E, 0x11, 0x11, 0x11,
        0x11, 0x11, 0x0E
    };

    static const uint8_t P[7] = {
        0x1E, 0x11, 0x11, 0x1E,
        0x10, 0x10, 0x10
    };

    static const uint8_t Q[7] = {
        0x0E, 0x11, 0x11, 0x11,
        0x15, 0x12, 0x0D
    };

    static const uint8_t R[7] = {
        0x1E, 0x11, 0x11, 0x1E,
        0x14, 0x12, 0x11
    };

    static const uint8_t S[7] = {
        0x0F, 0x10, 0x10, 0x0E,
        0x01, 0x01, 0x1E
    };

    static const uint8_t T[7] = {
        0x1F, 0x04, 0x04, 0x04,
        0x04, 0x04, 0x04
    };

    static const uint8_t U[7] = {
        0x11, 0x11, 0x11, 0x11,
        0x11, 0x11, 0x0E
    };

    static const uint8_t V[7] = {
        0x11, 0x11, 0x11, 0x11,
        0x11, 0x0A, 0x04
    };

    static const uint8_t W[7] = {
        0x11, 0x11, 0x11, 0x15,
        0x15, 0x1B, 0x11
    };

    static const uint8_t X[7] = {
        0x11, 0x11, 0x0A, 0x04,
        0x0A, 0x11, 0x11
    };

    static const uint8_t Y[7] = {
        0x11, 0x11, 0x0A, 0x04,
        0x04, 0x04, 0x04
    };

    static const uint8_t Z[7] = {
        0x1F, 0x01, 0x02, 0x04,
        0x08, 0x10, 0x1F
    };


    static const uint8_t zero[7] = {
        0x0E, 0x11, 0x13, 0x15,
        0x19, 0x11, 0x0E
    };

    static const uint8_t one[7] = {
        0x04, 0x0C, 0x04, 0x04,
        0x04, 0x04, 0x0E
    };

    static const uint8_t two[7] = {
        0x0E, 0x11, 0x01, 0x02,
        0x04, 0x08, 0x1F
    };

    static const uint8_t three[7] = {
        0x1E, 0x01, 0x01, 0x0E,
        0x01, 0x01, 0x1E
    };

    static const uint8_t four[7] = {
        0x02, 0x06, 0x0A, 0x12,
        0x1F, 0x02, 0x02
    };

    static const uint8_t five[7] = {
        0x1F, 0x10, 0x10, 0x1E,
        0x01, 0x01, 0x1E
    };

    static const uint8_t six[7] = {
        0x0E, 0x10, 0x10, 0x1E,
        0x11, 0x11, 0x0E
    };

    static const uint8_t seven[7] = {
        0x1F, 0x01, 0x02, 0x04,
        0x08, 0x08, 0x08
    };

    static const uint8_t eight[7] = {
        0x0E, 0x11, 0x11, 0x0E,
        0x11, 0x11, 0x0E
    };

    static const uint8_t nine[7] = {
        0x0E, 0x11, 0x11, 0x0F,
        0x01, 0x01, 0x0E
    };


    static const uint8_t colon[7] = {
        0x00, 0x04, 0x04, 0x00,
        0x04, 0x04, 0x00
    };

    static const uint8_t period[7] = {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x06, 0x06
    };

    static const uint8_t comma[7] = {
        0x00, 0x00, 0x00, 0x00,
        0x06, 0x04, 0x08
    };

    static const uint8_t exclamation[7] = {
        0x04, 0x04, 0x04, 0x04,
        0x04, 0x00, 0x04
    };

    static const uint8_t minus[7] = {
        0x00, 0x00, 0x00, 0x1F,
        0x00, 0x00, 0x00
    };

    static const uint8_t slash[7] = {
        0x01, 0x01, 0x02, 0x04,
        0x08, 0x10, 0x10
    };

    static const uint8_t underscore[7] = {
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x1F
    };

    static const uint8_t question[7] = {
        0x0E, 0x11, 0x01, 0x02,
        0x04, 0x00, 0x04
    };

    static const uint8_t backslash[7] = {
        0x10, 0x10, 0x08, 0x04,
        0x02, 0x01, 0x01
    };


    switch (c) {
        case ' ': return space;

        case 'A': return A;
        case 'B': return B;
        case 'C': return C;
        case 'D': return D;
        case 'E': return E;
        case 'F': return F;
        case 'G': return G;
        case 'H': return H;
        case 'I': return I;
        case 'J': return J;
        case 'K': return K;
        case 'L': return L;
        case 'M': return M;
        case 'N': return N;
        case 'O': return O;
        case 'P': return P;
        case 'Q': return Q;
        case 'R': return R;
        case 'S': return S;
        case 'T': return T;
        case 'U': return U;
        case 'V': return V;
        case 'W': return W;
        case 'X': return X;
        case 'Y': return Y;
        case 'Z': return Z;

        case '0': return zero;
        case '1': return one;
        case '2': return two;
        case '3': return three;
        case '4': return four;
        case '5': return five;
        case '6': return six;
        case '7': return seven;
        case '8': return eight;
        case '9': return nine;

        case ':': return colon;
        case '.': return period;
        case ',': return comma;
        case '!': return exclamation;
        case '-': return minus;
        case '/': return slash;
        case '_': return underscore;
        case '?': return question;
        case '\\': return backslash;

        default:
            return question;
    }
}


static char uppercase(char c)
{
    if (c >= 'a' && c <= 'z')
        return (char)(c - 'a' + 'A');

    return c;
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

static void draw_glyph(
    char c,
    uint32_t x,
    uint32_t y
)
{
    const uint8_t* glyph =
        get_glyph(uppercase(c));

    struct RGBColor fg =
        color_palette[foreground_color];

    struct RGBColor bg =
        color_palette[background_color];

    for (uint32_t row = 0;
         row < FONT_HEIGHT;
         row++) {

        for (uint32_t col = 0;
             col < FONT_WIDTH;
             col++) {

            uint8_t pixel =
                glyph[row] &
                (1u << (FONT_WIDTH - 1 - col));

            uint8_t r;
            uint8_t g;
            uint8_t b;

            if (pixel) {
                r = fg.r;
                g = fg.g;
                b = fg.b;
            } else {
                r = bg.r;
                g = bg.g;
                b = bg.b;
            }

            for (uint32_t sy = 0;
                 sy < FONT_SCALE;
                 sy++) {

                for (uint32_t sx = 0;
                     sx < FONT_SCALE;
                     sx++) {

                    fb_put_pixel(
                        x +
                        col * FONT_SCALE +
                        sx,

                        y +
                        row * FONT_SCALE +
                        sy,

                        r,
                        g,
                        b
                    );
                }
            }
        }
    }

    /*
     * Clear the spacing column.
     */
    for (uint32_t y2 = y;
         y2 < y + FONT_HEIGHT * FONT_SCALE;
         y2++) {

        fb_put_pixel(
            x + FONT_WIDTH * FONT_SCALE,
            y2,
            bg.r,
            bg.g,
            bg.b
        );
    }
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

    if (cursor_row < FB_MAX_ROWS)
        return;

    /*
     * Scroll the logical character buffer.
     */
    for (uint32_t row = 1;
         row < FB_MAX_ROWS;
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

        text_buffer[FB_MAX_ROWS - 1][col] = ' ';
    }

    cursor_row = FB_MAX_ROWS - 1;

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
     * Wrap at right edge.
     */
    if (cursor_x + CHAR_WIDTH >
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


    draw_glyph(
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
    cursor_x += CHAR_WIDTH;
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
        cursor_col * CHAR_WIDTH;

    cursor_y =
        cursor_row * CHAR_HEIGHT;


    /*
     * Erase the character.
     */
    struct RGBColor bg =
        color_palette[background_color];

    for (uint32_t y = cursor_y;
         y < cursor_y + CHAR_HEIGHT &&
         y < framebuffer.height;
         y++) {

        for (uint32_t x = cursor_x;
             x < cursor_x + CHAR_WIDTH &&
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

    uint32_t max_cols =
        framebuffer.width / CHAR_WIDTH;


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
        cursor_col * CHAR_WIDTH;

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
        cursor_col * CHAR_WIDTH;

    cursor_y =
        cursor_row * CHAR_HEIGHT;
}


void fbmove_cursor_right(void)
{
    uint32_t max_cols =
        framebuffer.width / CHAR_WIDTH;

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
        cursor_col * CHAR_WIDTH;

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