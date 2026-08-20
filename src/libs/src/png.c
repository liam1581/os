#include "drivers/files/image/png.h"

#include <stddef.h>
#include "string.h"
#include "mem/kheap.h"

/* ------------------------------------------------------------------ */
/* PNG chunk / IHDR parsing                                            */
/* ------------------------------------------------------------------ */

#define PNG_SIGNATURE_SIZE 8
static const uint8_t PNG_SIGNATURE[PNG_SIGNATURE_SIZE] = {
    0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'
};

static uint32_t read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

// Walks PNG chunks starting at cw->pos. Bounds-checked against
// cw->file_size on every call; advances cw->pos past the returned
// chunk on success. Returns false once there's no more valid
// (non-truncated) chunk to read.
typedef struct {
    const uint8_t* data;
    uint32_t pos;
    uint32_t file_size;
} ChunkWalker;

static bool chunk_walker_next(ChunkWalker* cw, uint32_t* out_len, const uint8_t** out_type, const uint8_t** out_data) {
    if ((uint64_t)cw->pos + 8 > cw->file_size) return false;

    uint32_t len = read_be32(cw->data + cw->pos);
    if ((uint64_t)cw->pos + 8 + (uint64_t)len + 4 > cw->file_size) return false; // truncated chunk

    *out_len = len;
    *out_type = cw->data + cw->pos + 4;
    *out_data = cw->data + cw->pos + 8;

    cw->pos += 8 + len + 4; // length + type + data + crc
    return true;
}

static bool chunk_type_is(const uint8_t* type, char a, char b, char c, char d) {
    return type[0] == a && type[1] == b && type[2] == c && type[3] == d;
}

bool png_has_alpha(const PNGHeader* header) {
    if (header->file.color_type == 4 || header->file.color_type == 6) return true;

    if (header->file.color_type == 3) {
        for (uint16_t i = 0; i < header->file.palette_count; i++) {
            if (header->file.palette_alpha[i] != 255) return true;
        }
    }

    return false;
}

bool validate_png_header(const uint8_t* data, uint32_t file_size, PNGHeader* header) {
    if (data == NULL || header == NULL) return false;

    // Signature (8 bytes) + first chunk's length(4) + type(4) + IHDR data(13) + CRC(4)
    if (file_size < PNG_SIGNATURE_SIZE + 4 + 4 + 13 + 4) return false;

    for (int i = 0; i < PNG_SIGNATURE_SIZE; i++) {
        if (data[i] != PNG_SIGNATURE[i]) return false;
    }

    const uint8_t* p = data + PNG_SIGNATURE_SIZE;

    uint32_t chunk_len = read_be32(p);
    const uint8_t* chunk_type = p + 4;
    const uint8_t* chunk_data = p + 8;

    // The very first chunk must be IHDR, and it's always exactly 13 bytes.
    if (chunk_type[0] != 'I' || chunk_type[1] != 'H' ||
        chunk_type[2] != 'D' || chunk_type[3] != 'R') return false;
    if (chunk_len != 13) return false;

    header->file.width         = read_be32(chunk_data + 0);
    header->file.height        = read_be32(chunk_data + 4);
    header->file.bit_depth     = chunk_data[8];
    header->file.color_type    = chunk_data[9];
    header->file.compression   = chunk_data[10];
    header->file.filter_method = chunk_data[11];
    header->file.interlace     = chunk_data[12];

    header->file.palette_count = 0;
    for (int i = 0; i < 256; i++) header->file.palette_alpha[i] = 255; // opaque unless tRNS says otherwise

    if (header->file.width == 0 || header->file.height == 0) return false;


    // Guard against a corrupt/adversarial width*height*bpp overflowing
    // the size_t multiplication later on when we size the pixel buffer.
    if (header->file.width > 0x7FFFFFFFu || header->file.height > 0x7FFFFFFFu) return false;

    if (header->file.compression != 0) return false;             // only DEFLATE is defined anyway
    if (header->file.filter_method != 0) return false;           // only method 0 is defined
    if (header->file.interlace != 0) return false;                // Adam7 not supported

    // Valid bit depths per color type (PNG spec table 11.1).
    switch (header->file.color_type) {
        case 0: // grayscale
            if (header->file.bit_depth != 1 && header->file.bit_depth != 2 && header->file.bit_depth != 4 &&
                header->file.bit_depth != 8 && header->file.bit_depth != 16) return false;
            break;
        case 2: // RGB
        case 4: // grayscale + alpha
        case 6: // RGBA
            if (header->file.bit_depth != 8 && header->file.bit_depth != 16) return false;
            break;
        case 3: // palette
            if (header->file.bit_depth != 1 && header->file.bit_depth != 2 &&
                header->file.bit_depth != 4 && header->file.bit_depth != 8) return false;
            break;
        default:
            return false; // unknown color type
    }

    if (header->file.color_type == 3) {
        // Palette images require a PLTE chunk; walk chunks after IHDR
        // looking for it (and an optional tRNS), stopping once IDAT
        // starts (PLTE/tRNS are required to precede IDAT).
        ChunkWalker cw = { data, PNG_SIGNATURE_SIZE + 8 + 13 + 4, file_size };
        bool found_plte = false;

        uint32_t clen;
        const uint8_t* ctype;
        const uint8_t* cdata;

        while (chunk_walker_next(&cw, &clen, &ctype, &cdata)) {
            if (chunk_type_is(ctype, 'I', 'D', 'A', 'T')) break;
            if (chunk_type_is(ctype, 'I', 'E', 'N', 'D')) break;

            if (chunk_type_is(ctype, 'P', 'L', 'T', 'E')) {
                if (clen == 0 || clen % 3 != 0) return false;
                uint32_t count = clen / 3;
                if (count > 256) return false;

                for (uint32_t i = 0; i < count; i++) {
                    header->file.palette[i][0] = cdata[i * 3 + 0];
                    header->file.palette[i][1] = cdata[i * 3 + 1];
                    header->file.palette[i][2] = cdata[i * 3 + 2];
                }
                header->file.palette_count = (uint16_t)count;
                found_plte = true;
            } else if (chunk_type_is(ctype, 't', 'R', 'N', 'S')) {
                if (!found_plte) return false; // tRNS must follow PLTE
                if (clen > header->file.palette_count) return false;

                for (uint32_t i = 0; i < clen; i++) {
                    header->file.palette_alpha[i] = cdata[i];
                }
            }
        }

        if (!found_plte) return false;
    }

    return true;
}

/* ------------------------------------------------------------------ */
/* Bit reader (LSB-first, per RFC 1951 section 3.1.1)                  */
/* ------------------------------------------------------------------ */

typedef struct {
    const uint8_t* data;
    uint32_t size;
    uint32_t byte_pos;
    uint32_t bit_buf;
    int bit_count;
} BitReader;

static void bitreader_init(BitReader* br, const uint8_t* data, uint32_t size) {
    br->data = data;
    br->size = size;
    br->byte_pos = 0;
    br->bit_buf = 0;
    br->bit_count = 0;
}

// Returns -1 on end-of-input.
static int bitreader_get_bit(BitReader* br) {
    if (br->bit_count == 0) {
        if (br->byte_pos >= br->size) return -1;
        br->bit_buf = br->data[br->byte_pos++];
        br->bit_count = 8;
    }
    int bit = br->bit_buf & 1;
    br->bit_buf >>= 1;
    br->bit_count--;
    return bit;
}

// Reads `count` bits (0-24), LSB first, combined value returned LSB-first
// (i.e. the first bit read is the low-order bit). Returns -1 on
// end-of-input. count == 0 returns 0.
static int32_t bitreader_get_bits(BitReader* br, int count) {
    int32_t value = 0;
    for (int i = 0; i < count; i++) {
        int bit = bitreader_get_bit(br);
        if (bit < 0) return -1;
        value |= (bit << i);
    }
    return value;
}

static void bitreader_align_to_byte(BitReader* br) {
    br->bit_buf = 0;
    br->bit_count = 0;
}

/* ------------------------------------------------------------------ */
/* Canonical Huffman decoding                                          */
/*                                                                      */
/* Same approach as RFC 1951's reference decoder (and the well-known    */
/* public-domain puff.c): store per-length symbol counts plus a symbol  */
/* table sorted by (length, symbol), then walk bit-by-bit comparing     */
/* against the first code of each length. All tables are small (at     */
/* most 288 literal/length symbols, 16 lengths) so these are safe as    */
/* plain locals -- nothing here is stack-heavy.                        */
/* ------------------------------------------------------------------ */

#define HUFFMAN_MAX_BITS 15
#define HUFFMAN_MAX_LIT_SYMBOLS 288
#define HUFFMAN_MAX_DIST_SYMBOLS 32

typedef struct {
    uint16_t counts[HUFFMAN_MAX_BITS + 1]; // counts[n] = number of codes of length n
    uint16_t symbols[HUFFMAN_MAX_LIT_SYMBOLS]; // symbols sorted by (length, symbol)
    int num_symbols;
} HuffmanTree;

static bool huffman_build(HuffmanTree* tree, const uint8_t* lengths, int num_symbols) {
    if (num_symbols > HUFFMAN_MAX_LIT_SYMBOLS) return false;

    for (int i = 0; i <= HUFFMAN_MAX_BITS; i++) tree->counts[i] = 0;
    for (int i = 0; i < num_symbols; i++) {
        if (lengths[i] > HUFFMAN_MAX_BITS) return false;
        tree->counts[lengths[i]]++;
    }
    tree->counts[0] = 0; // length-0 symbols don't participate in the code

    uint16_t offsets[HUFFMAN_MAX_BITS + 2];
    offsets[1] = 0;
    for (int len = 1; len <= HUFFMAN_MAX_BITS; len++) {
        offsets[len + 1] = offsets[len] + tree->counts[len];
    }

    for (int i = 0; i < num_symbols; i++) {
        if (lengths[i] != 0) {
            tree->symbols[offsets[lengths[i]]++] = (uint16_t)i;
        }
    }

    tree->num_symbols = num_symbols;
    return true;
}

// Decodes one symbol. Returns -1 on end-of-input or an invalid code.
static int32_t huffman_decode(BitReader* br, const HuffmanTree* tree) {
    int code = 0;
    int first = 0;
    int index = 0;

    for (int len = 1; len <= HUFFMAN_MAX_BITS; len++) {
        int bit = bitreader_get_bit(br);
        if (bit < 0) return -1;

        code |= bit;
        int count = tree->counts[len];

        if (code - first < count) {
            return tree->symbols[index + (code - first)];
        }

        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }

    return -1; // no matching code of any length -- corrupt stream
}

/* ------------------------------------------------------------------ */
/* DEFLATE (RFC 1951)                                                   */
/* ------------------------------------------------------------------ */

// Length/distance extra-bit tables (RFC 1951 section 3.2.5).
static const uint16_t LENGTH_BASE[29] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,
    35,43,51,59,67,83,99,115,131,163,195,227,258
};
static const uint8_t LENGTH_EXTRA_BITS[29] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,
    3,3,3,3,4,4,4,4,5,5,5,5,0
};
static const uint16_t DIST_BASE[30] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
    257,385,513,769,1025,1537,2049,3073,4097,6145,
    8193,12289,16385,24577
};
static const uint8_t DIST_EXTRA_BITS[30] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,
    7,7,8,8,9,9,10,10,11,11,12,12,13,13
};

// Order code-length code-lengths are transmitted in for dynamic blocks.
static const uint8_t CODE_LENGTH_ORDER[19] = {
    16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15
};

typedef struct {
    uint8_t* buf;
    uint32_t size;     // allocated capacity
    uint32_t pos;       // bytes written so far
} ByteBuffer;

static bool bytebuffer_ensure(ByteBuffer* out, uint32_t extra) {
    if (out->pos + extra <= out->size) return true;

    uint32_t new_size = out->size ? out->size * 2 : 4096;
    while (new_size < out->pos + extra) new_size *= 2;

    uint8_t* new_buf = (uint8_t*)kmalloc(new_size);
    if (!new_buf) return false;

    if (out->pos > 0) memcpy(new_buf, out->buf, out->pos);
    if (out->buf) kfree(out->buf);

    out->buf = new_buf;
    out->size = new_size;
    return true;
}

static bool bytebuffer_put(ByteBuffer* out, uint8_t byte) {
    if (!bytebuffer_ensure(out, 1)) return false;
    out->buf[out->pos++] = byte;
    return true;
}

static void build_fixed_trees(HuffmanTree* lit_tree, HuffmanTree* dist_tree) {
    uint8_t lit_lengths[288];
    for (int i = 0;   i <= 143; i++) lit_lengths[i] = 8;
    for (int i = 144; i <= 255; i++) lit_lengths[i] = 9;
    for (int i = 256; i <= 279; i++) lit_lengths[i] = 7;
    for (int i = 280; i <= 287; i++) lit_lengths[i] = 8;
    huffman_build(lit_tree, lit_lengths, 288);

    uint8_t dist_lengths[30];
    for (int i = 0; i < 30; i++) dist_lengths[i] = 5;
    huffman_build(dist_tree, dist_lengths, 30);
}

static bool read_dynamic_trees(BitReader* br, HuffmanTree* lit_tree, HuffmanTree* dist_tree) {
    int32_t hlit  = bitreader_get_bits(br, 5);
    int32_t hdist = bitreader_get_bits(br, 5);
    int32_t hclen = bitreader_get_bits(br, 4);
    if (hlit < 0 || hdist < 0 || hclen < 0) return false;

    hlit  += 257;
    hdist += 1;
    hclen += 4;

    uint8_t cl_lengths[19] = {0};
    for (int i = 0; i < hclen; i++) {
        int32_t v = bitreader_get_bits(br, 3);
        if (v < 0) return false;
        cl_lengths[CODE_LENGTH_ORDER[i]] = (uint8_t)v;
    }

    HuffmanTree cl_tree;
    if (!huffman_build(&cl_tree, cl_lengths, 19)) return false;

    uint8_t lengths[HUFFMAN_MAX_LIT_SYMBOLS + HUFFMAN_MAX_DIST_SYMBOLS];
    int total = hlit + hdist;
    if (total > (int)(sizeof(lengths))) return false;

    int i = 0;
    while (i < total) {
        int32_t sym = huffman_decode(br, &cl_tree);
        if (sym < 0) return false;

        if (sym < 16) {
            lengths[i++] = (uint8_t)sym;
        } else if (sym == 16) {
            if (i == 0) return false; // nothing to repeat
            int32_t rep = bitreader_get_bits(br, 2);
            if (rep < 0) return false;
            rep += 3;
            uint8_t prev = lengths[i - 1];
            while (rep-- > 0 && i < total) lengths[i++] = prev;
        } else if (sym == 17) {
            int32_t rep = bitreader_get_bits(br, 3);
            if (rep < 0) return false;
            rep += 3;
            while (rep-- > 0 && i < total) lengths[i++] = 0;
        } else { // 18
            int32_t rep = bitreader_get_bits(br, 7);
            if (rep < 0) return false;
            rep += 11;
            while (rep-- > 0 && i < total) lengths[i++] = 0;
        }
    }

    if (!huffman_build(lit_tree, lengths, hlit)) return false;
    if (!huffman_build(dist_tree, lengths + hlit, hdist)) return false;
    return true;
}

static bool inflate_block_stored(BitReader* br, ByteBuffer* out) {
    bitreader_align_to_byte(br);

    if (br->byte_pos + 4 > br->size) return false;
    uint16_t len  = (uint16_t)(br->data[br->byte_pos] | (br->data[br->byte_pos + 1] << 8));
    uint16_t nlen = (uint16_t)(br->data[br->byte_pos + 2] | (br->data[br->byte_pos + 3] << 8));
    br->byte_pos += 4;

    if ((uint16_t)(~len) != nlen) return false;
    if (br->byte_pos + len > br->size) return false;

    if (!bytebuffer_ensure(out, len)) return false;
    memcpy(out->buf + out->pos, br->data + br->byte_pos, len);
    out->pos += len;
    br->byte_pos += len;
    return true;
}

static bool inflate_block_huffman(BitReader* br, ByteBuffer* out, const HuffmanTree* lit_tree, const HuffmanTree* dist_tree) {
    for (;;) {
        int32_t sym = huffman_decode(br, lit_tree);
        if (sym < 0) return false;

        if (sym < 256) {
            if (!bytebuffer_put(out, (uint8_t)sym)) return false;
        } else if (sym == 256) {
            return true; // end of block
        } else {
            int idx = sym - 257;
            if (idx >= 29) return false;

            int32_t extra = bitreader_get_bits(br, LENGTH_EXTRA_BITS[idx]);
            if (extra < 0) return false;
            uint32_t length = LENGTH_BASE[idx] + (uint32_t)extra;

            int32_t dist_sym = huffman_decode(br, dist_tree);
            if (dist_sym < 0 || dist_sym >= 30) return false;

            int32_t dist_extra = bitreader_get_bits(br, DIST_EXTRA_BITS[dist_sym]);
            if (dist_extra < 0) return false;
            uint32_t distance = DIST_BASE[dist_sym] + (uint32_t)dist_extra;

            if (distance == 0 || distance > out->pos) return false; // back-ref before start of output

            if (!bytebuffer_ensure(out, length)) return false;
            uint32_t src = out->pos - distance;
            for (uint32_t k = 0; k < length; k++) {
                out->buf[out->pos] = out->buf[src + k]; // byte-by-byte: source can overlap dest (LZ77 run-length)
                out->pos++;
            }
        }
    }
}

// Inflates a raw DEFLATE stream (no zlib wrapper) into a growable
// ByteBuffer. Returns false on any malformed/unsupported input.
static bool inflate_raw(const uint8_t* data, uint32_t size, ByteBuffer* out) {
    BitReader br;
    bitreader_init(&br, data, size);

    for (;;) {
        int32_t is_final = bitreader_get_bit(&br);
        if (is_final < 0) return false;
        int32_t type = bitreader_get_bits(&br, 2);
        if (type < 0) return false;

        bool ok;
        if (type == 0) {
            ok = inflate_block_stored(&br, out);
        } else if (type == 1) {
            HuffmanTree lit_tree, dist_tree;
            build_fixed_trees(&lit_tree, &dist_tree);
            ok = inflate_block_huffman(&br, out, &lit_tree, &dist_tree);
        } else if (type == 2) {
            HuffmanTree lit_tree, dist_tree;
            ok = read_dynamic_trees(&br, &lit_tree, &dist_tree);
            if (ok) ok = inflate_block_huffman(&br, out, &lit_tree, &dist_tree);
        } else {
            ok = false; // type 3 is reserved/invalid
        }

        if (!ok) return false;
        if (is_final) return true;
    }
}

/* ------------------------------------------------------------------ */
/* zlib wrapper (RFC 1950) -- PNG's IDAT stream is zlib-wrapped DEFLATE */
/* ------------------------------------------------------------------ */

static uint32_t adler32(const uint8_t* data, uint32_t size) {
    uint32_t a = 1, b = 0;
    const uint32_t MOD_ADLER = 65521;
    for (uint32_t i = 0; i < size; i++) {
        a = (a + data[i]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }
    return (b << 16) | a;
}

static bool zlib_inflate(const uint8_t* data, uint32_t size, ByteBuffer* out) {
    if (size < 6) return false; // 2-byte header + at least empty deflate stream + 4-byte adler32

    uint8_t cmf = data[0];
    uint8_t flg = data[1];
    if ((cmf & 0x0F) != 8) return false;              // compression method must be DEFLATE
    if (((uint32_t)cmf * 256 + flg) % 31 != 0) return false; // header checksum
    if (flg & 0x20) return false;                       // preset dictionary not supported

    if (!inflate_raw(data + 2, size - 6, out)) return false;

    uint32_t expected = read_be32(data + size - 4);
    uint32_t actual = adler32(out->buf, out->pos);
    if (expected != actual) return false;

    return true;
}

/* ------------------------------------------------------------------ */
/* PNG scanline de-filtering (RFC 2083 section 6)                      */
/* ------------------------------------------------------------------ */

static uint8_t paeth_predictor(uint8_t a, uint8_t b, uint8_t c) {
    int p = (int)a + (int)b - (int)c;
    int pa = p > a ? p - a : a - p;
    int pb = p > b ? p - b : b - p;
    int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

// filter_bpp is "bytes per complete pixel, rounded up to a minimum of 1"
// per RFC 2083 section 6.3 -- for sub-byte bit depths, filtering
// operates on the raw byte stream (distance = 1 byte back), not on
// individual sub-byte pixel boundaries. row_bytes is the packed
// per-scanline byte count (which for depth < 8 is NOT simply
// width * channels).
static bool unfilter(const uint8_t* filtered, uint32_t height, uint32_t row_bytes, uint32_t filter_bpp, uint8_t* out_raw) {
    uint32_t stride = row_bytes + 1; // +1 for the filter-type byte prefix

    for (uint32_t y = 0; y < height; y++) {
        const uint8_t* row_in = filtered + (uint64_t)y * stride;
        uint8_t filter_type = row_in[0];
        const uint8_t* src = row_in + 1;
        uint8_t* dst = out_raw + (uint64_t)y * row_bytes;
        const uint8_t* prev = (y == 0) ? NULL : (out_raw + (uint64_t)(y - 1) * row_bytes);

        for (uint32_t x = 0; x < row_bytes; x++) {
            uint8_t a = (x >= filter_bpp) ? dst[x - filter_bpp] : 0;               // left
            uint8_t b = prev ? prev[x] : 0;                                          // up
            uint8_t c = (prev && x >= filter_bpp) ? prev[x - filter_bpp] : 0;       // upper-left

            switch (filter_type) {
                case 0: dst[x] = src[x]; break;                              // None
                case 1: dst[x] = (uint8_t)(src[x] + a); break;                 // Sub
                case 2: dst[x] = (uint8_t)(src[x] + b); break;                 // Up
                case 3: dst[x] = (uint8_t)(src[x] + ((a + b) / 2)); break;      // Average
                case 4: dst[x] = (uint8_t)(src[x] + paeth_predictor(a, b, c)); break; // Paeth
                default: return false; // invalid filter type
            }
        }
    }

    return true;
}

// Scales a 1/2/4/8-bit sample to the full 0-255 range. Exact (no
// rounding error) for every standard PNG bit depth, since 255 divides
// evenly by each possible max_sample (1, 3, 15, 255).
static uint8_t scale_sample_to_8bit(uint32_t sample, uint8_t bit_depth) {
    uint32_t max_sample = (1u << bit_depth) - 1;
    return (uint8_t)((sample * 255u) / max_sample);
}

// Extracts the sample_index-th sample (0-based, MSB-first within each
// byte) from a row packed at the given sub-8-bit depth.
static uint32_t extract_packed_sample(const uint8_t* row, uint32_t sample_index, uint8_t bit_depth) {
    uint32_t samples_per_byte = 8 / bit_depth;
    uint32_t byte_index = sample_index / samples_per_byte;
    uint32_t sample_in_byte = sample_index % samples_per_byte;
    uint32_t shift = 8 - bit_depth - (sample_in_byte * bit_depth);
    uint32_t mask = (1u << bit_depth) - 1;
    return (row[byte_index] >> shift) & mask;
}

// Converts de-filtered, source-format scanlines (still packed per the
// PNG's own bit depth/color type) into the uniform tightly packed 8-bit
// RGB or RGBA output buffer png_decode() promises callers.
static bool expand_to_rgb(const uint8_t* raw, const PNGHeader* header, uint8_t* out_pixels) {
    uint32_t width = header->file.width;
    uint32_t height = header->file.height;
    uint8_t bit_depth = header->file.bit_depth;
    uint8_t color_type = header->file.color_type;
    bool has_alpha = png_has_alpha(header);
    uint8_t out_bpp = has_alpha ? 4 : 3;

    uint8_t channels;
    switch (color_type) {
        case 0: channels = 1; break; // grayscale
        case 2: channels = 3; break; // RGB
        case 3: channels = 1; break; // palette (index)
        case 4: channels = 2; break; // grayscale+alpha
        case 6: channels = 4; break; // RGBA
        default: return false;
    }

    uint32_t row_bytes = (uint32_t)(((uint64_t)width * channels * bit_depth + 7) / 8);

    for (uint32_t y = 0; y < height; y++) {
        const uint8_t* row = raw + (uint64_t)y * row_bytes;
        uint8_t* out_row = out_pixels + (uint64_t)y * width * out_bpp;

        for (uint32_t x = 0; x < width; x++) {
            uint8_t* out_pixel = out_row + x * out_bpp;

            if (color_type == 3) {
                // Palette: one sample per pixel, always <= 8 bits.
                uint32_t index = (bit_depth == 8)
                    ? row[x]
                    : extract_packed_sample(row, x, bit_depth);

                if (index >= header->file.palette_count) return false; // corrupt: index outside PLTE

                out_pixel[0] = header->file.palette[index][0];
                out_pixel[1] = header->file.palette[index][1];
                out_pixel[2] = header->file.palette[index][2];
                if (has_alpha) out_pixel[3] = header->file.palette_alpha[index];
                continue;
            }

            // Grayscale / RGB / grayscale+alpha / RGBA: 8 or 16 bits per channel.
            uint8_t sample_bytes[4];
            for (uint8_t ch = 0; ch < channels; ch++) {
                uint32_t sample;
                if (bit_depth == 16) {
                    uint32_t offset = (x * channels + ch) * 2;
                    sample = ((uint32_t)row[offset] << 8) | row[offset + 1];
                    sample_bytes[ch] = (uint8_t)(sample >> 8); // downsample: take the high byte
                } else if (bit_depth == 8) {
                    sample_bytes[ch] = row[x * channels + ch];
                } else {
                    // Only color_type 0 (grayscale) can have sub-8-bit
                    // depth outside the palette case, per IHDR validation.
                    sample = extract_packed_sample(row, x, bit_depth);
                    sample_bytes[ch] = scale_sample_to_8bit(sample, bit_depth);
                }
            }

            if (color_type == 0) { // grayscale -> replicate to R=G=B
                out_pixel[0] = out_pixel[1] = out_pixel[2] = sample_bytes[0];
            } else if (color_type == 4) { // grayscale+alpha
                out_pixel[0] = out_pixel[1] = out_pixel[2] = sample_bytes[0];
                out_pixel[3] = sample_bytes[1];
            } else if (color_type == 2) { // RGB
                out_pixel[0] = sample_bytes[0];
                out_pixel[1] = sample_bytes[1];
                out_pixel[2] = sample_bytes[2];
            } else { // RGBA (6)
                out_pixel[0] = sample_bytes[0];
                out_pixel[1] = sample_bytes[1];
                out_pixel[2] = sample_bytes[2];
                out_pixel[3] = sample_bytes[3];
            }
        }
    }

    return true;
}

/* ------------------------------------------------------------------ */
/* Public entry point                                                   */
/* ------------------------------------------------------------------ */

bool png_decode(const uint8_t* data, uint32_t file_size, const PNGHeader* header, uint8_t** out_pixels, uint32_t* out_size) {
    if (!data || !header || !out_pixels || !out_size) return false;

    // --- Pass 1: walk chunks, sum up total IDAT payload size ---
    uint32_t idat_total = 0;
    bool saw_iend = false;

    {
        ChunkWalker cw = { data, PNG_SIGNATURE_SIZE, file_size };
        uint32_t clen;
        const uint8_t* ctype;
        const uint8_t* cdata;

        while (chunk_walker_next(&cw, &clen, &ctype, &cdata)) {
            if (chunk_type_is(ctype, 'I', 'D', 'A', 'T')) {
                idat_total += clen;
            } else if (chunk_type_is(ctype, 'I', 'E', 'N', 'D')) {
                saw_iend = true;
                break;
            }
        }
    }

    if (!saw_iend || idat_total == 0) return false;

    // --- Pass 2: concatenate IDAT payloads (a PNG may split the zlib
    //     stream across multiple IDAT chunks) ---
    uint8_t* idat_buf = (uint8_t*)kmalloc(idat_total);
    if (!idat_buf) return false;

    {
        uint32_t idat_pos = 0;
        ChunkWalker cw = { data, PNG_SIGNATURE_SIZE, file_size };
        uint32_t clen;
        const uint8_t* ctype;
        const uint8_t* cdata;

        while (chunk_walker_next(&cw, &clen, &ctype, &cdata)) {
            if (chunk_type_is(ctype, 'I', 'D', 'A', 'T')) {
                memcpy(idat_buf + idat_pos, cdata, clen);
                idat_pos += clen;
            } else if (chunk_type_is(ctype, 'I', 'E', 'N', 'D')) {
                break;
            }
        }
    }

    // --- Inflate the zlib-wrapped DEFLATE stream ---
    ByteBuffer inflated = {0};
    bool ok = zlib_inflate(idat_buf, idat_total, &inflated);
    kfree(idat_buf);

    if (!ok) {
        if (inflated.buf) kfree(inflated.buf);
        return false;
    }

    // --- De-filter into a raw, source-format buffer ---
    //
    // channels/row_bytes/filter_bpp describe the SOURCE format (as
    // stored in the file -- e.g. 1 channel for palette/grayscale, and
    // row_bytes accounting for sub-8-bit packing), not the final RGB/
    // RGBA output. See expand_to_rgb() for the format conversion step.
    uint8_t channels;
    switch (header->file.color_type) {
        case 0: channels = 1; break;
        case 2: channels = 3; break;
        case 3: channels = 1; break;
        case 4: channels = 2; break;
        case 6: channels = 4; break;
        default: kfree(inflated.buf); return false;
    }

    uint32_t row_bytes = (uint32_t)(((uint64_t)header->file.width * channels * header->file.bit_depth + 7) / 8);
    uint32_t filter_bpp = (channels * header->file.bit_depth + 7) / 8;
    if (filter_bpp < 1) filter_bpp = 1;

    uint64_t expected_filtered_size = (uint64_t)(row_bytes + 1) * header->file.height;
    if (inflated.pos < expected_filtered_size) {
        kfree(inflated.buf);
        return false;
    }

    uint64_t raw_buf_size = (uint64_t)row_bytes * header->file.height;
    uint8_t* raw = (uint8_t*)kmalloc((size_t)raw_buf_size);
    if (!raw) {
        kfree(inflated.buf);
        return false;
    }

    ok = unfilter(inflated.buf, header->file.height, row_bytes, filter_bpp, raw);
    kfree(inflated.buf);

    if (!ok) {
        kfree(raw);
        return false;
    }

    // --- Expand the de-filtered source-format data into the final
    //     tightly packed, uniform 8-bit RGB/RGBA output buffer ---
    uint8_t out_bpp = png_output_bytes_per_pixel(header);
    uint64_t pixel_buf_size = (uint64_t)header->file.width * header->file.height * out_bpp;

    uint8_t* pixels = (uint8_t*)kmalloc((size_t)pixel_buf_size);
    if (!pixels) {
        kfree(raw);
        return false;
    }

    ok = expand_to_rgb(raw, header, pixels);
    kfree(raw);

    if (!ok) {
        kfree(pixels);
        return false;
    }

    *out_pixels = pixels;
    *out_size = (uint32_t)pixel_buf_size;
    return true;
}
