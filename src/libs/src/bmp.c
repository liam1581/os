#include "drivers/files/image/bmp.h"

#include <stddef.h>
#include "print.h"
#include "string.h"

bool validate_bmp_header(const uint8_t *data, uint32_t file_size, BMPHeader *header) {
    if (data == NULL) {
        println("PIXEL DATA IS ZERO");
        return false;
    }

    if (file_size < sizeof(BMPHeader)) {
        println("FILE SIZE IS SMALLER THAN HEADER SIZE");
        return false;
    }

    memcpy(&header->file, data, sizeof(BMPFileHeader));
    memcpy(
        &header->info,
        data + sizeof(BMPFileHeader),
        sizeof(BMPInfoHeader)
    );

    if (header->file.signature != 0x4D42) {
        println("INVALID FILE SIGNATURE");
        return false;
    }

    if (header->file.file_size != file_size) {
        println("FILE SIZE IN HEADER DOESNT MATCH REAL FILE SIZE");
        return false;
    }

    if (header->file.reserved1 != 0 ||
        header->file.reserved2 != 0) {
            println("RESERVED BYTES ARENT 0");
            return false;
        }

    if (header->file.pixel_offset < sizeof(BMPHeader)) {
        println("PIXEL DATA STARTS BEFORE THE HEADER");
        return false;
    }

    if (header->file.pixel_offset >= file_size) {
        println("PIXEL DATA OVERFLOW");
        return false;
    }

    if (header->info.header_size != 40) {
        println("BMP INFO HEADER IS TOO SHORT");
        return false;
    }

    if (header->info.width <= 0) {
        println("WIDTH MUST BE POSITIVE");
        return false;
    }

    if (header->info.height == 0) {
        println("HEIGHT CANT BE 0");
        return false;
    }

    if (header->info.planes != 1) {
        println("TOO MANY PLANES");
        return false;
    }

    if (header->info.bits_per_pixel != 24) {
        println("ONLY 24BPP IS SUPPORTED");
        return false;
    }

    if (header->info.compression != 0) {
        println("BI_RGB CANT BE COMPRESSED");
        return false;
    }

    uint64_t row_size = ((uint64_t)header->info.width * 3 + 3) & ~3ULL;

    uint64_t height;

    if (header->info.height < 0)
        height = -(int64_t)header->info.height;
    else
        height = header->info.height;

    uint64_t pixel_data_size = row_size * height;

    if ((uint64_t)header->file.pixel_offset + pixel_data_size > file_size) {
        println("PIXEL DATA TOO LONG FOR FILE");
        return false;
    }

    if (header->info.image_size != 0 && header->info.image_size < pixel_data_size) {
        println("IMAGE IS TOO SMALL");
        return false;
    }


    return true;
}