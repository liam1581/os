#include "drivers/files/image/bmp.h"

#include <stddef.h>
#include "string.h"

bool validate_bmp_header(const uint8_t *data, uint32_t file_size, BMPHeader *header) {
    // Need at least:
    // 14 bytes BMP file header
    // 40 bytes BITMAPINFOHEADER
    // = 54 bytes
    if (data == NULL)
        return false;

    if (file_size < sizeof(BMPHeader))
        return false;


    /*
     * Copy the bytes into our structs.
     *
     * BMP uses little-endian integers.
     * This works directly on normal little-endian machines.
     */
    memcpy(&header->file, data, sizeof(BMPFileHeader));
    memcpy(
        &header->info,
        data + sizeof(BMPFileHeader),
        sizeof(BMPInfoHeader)
    );


    /* -------------------- */
    /* Validate file header */
    /* -------------------- */

    // "BM"
    if (header->file.signature != 0x4D42)
        return false;

    // File size stored in BMP should match our actual size.
    if (header->file.file_size != file_size)
        return false;

    // Reserved fields must be zero.
    if (header->file.reserved1 != 0 ||
        header->file.reserved2 != 0)
        return false;

    // Pixel data cannot be before the headers.
    if (header->file.pixel_offset < sizeof(BMPHeader))
        return false;

    // Pixel offset cannot be outside the file.
    if (header->file.pixel_offset >= file_size)
        return false;


    /* -------------------- */
    /* Validate info header */
    /* -------------------- */

    // BITMAPINFOHEADER is 40 bytes.
    if (header->info.header_size != 40)
        return false;

    // Width must be positive.
    if (header->info.width <= 0)
        return false;

    // Height can be positive or negative.
    // Positive = bottom-up
    // Negative = top-down
    if (header->info.height == 0)
        return false;

    // BMP requires exactly one plane.
    if (header->info.planes != 1)
        return false;

    // For this implementation, only support 24-bit BMPs.
    if (header->info.bits_per_pixel != 24)
        return false;

    // BI_RGB = uncompressed.
    if (header->info.compression != 0)
        return false;


    /*
     * Calculate the size of one row.
     *
     * 24-bit BMP:
     *
     *     width * 3 bytes per pixel
     *
     * BMP rows are padded to a multiple of 4 bytes.
     */
    uint64_t row_size =
        ((uint64_t)header->info.width * 3 + 3) & ~3ULL;

    /*
     * abs(height), without overflowing if height == INT32_MIN.
     */
    uint64_t height;

    if (header->info.height < 0)
        height = -(int64_t)header->info.height;
    else
        height = header->info.height;

    /*
     * Total pixel data size.
     */
    uint64_t pixel_data_size = row_size * height;


    /*
     * Check that the pixel data actually fits inside
     * the supplied uint8_t[].
     */
    if ((uint64_t)header->file.pixel_offset +
            pixel_data_size >
        file_size)
    {
        return false;
    }


    /*
     * image_size is allowed to be zero for BI_RGB BMPs.
     *
     * If it is non-zero, make sure it is large enough.
     */
    if (header->info.image_size != 0 &&
        header->info.image_size < pixel_data_size)
    {
        return false;
    }


    return true;
}