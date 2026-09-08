#pragma once

#include <stdint.h>
#include <stddef.h>

extern "C" {
    #include <stdint.h>
    #include "bool.h"
    #include "print.h"
    #include "mem/mem.h"
    #include "drivers/storage/iso9660.h"
    #include "drivers/video/framebuffer.h"
    #include "drivers/files/image/bmp.h"
    #include "drivers/files/image/png.h"
}

enum class ImageType {
    BMP_IMAGE,
    PNG_IMAGE,
    JPG_IMAGE
};

void renderTexture(ImageType imageType, const char* path, uint32_t start_x, uint32_t start_y) {
    uint8_t* imgBuffer = (uint8_t*)kmalloc((size_t)iso9660_get_file_size(path));
    uint32_t outSize;

    if (iso9660_read_file(path, imgBuffer, &outSize)) {
        switch (imageType) {
        case ImageType::BMP_IMAGE: {
            BMPHeader header;

            if (validate_bmp_header(imgBuffer, outSize, &header)) {
                uint8_t *pixels = imgBuffer + header.file.pixel_offset;

                if (!framebuffer_draw_bmp(pixels, start_x, start_y, header.info.width, header.info.height)) {
                    println("Failed to draw bmp");
                }
            } else {
                println("Invalid BMP header");
            }
            break;
        }
        case ImageType::PNG_IMAGE: {
            PNGHeader pngHeader;

            if (validate_png_header(imgBuffer, outSize, &pngHeader)) {
                uint8_t* pixels;
                uint32_t pixelSize;

                if (png_decode(imgBuffer, outSize, &pngHeader, &pixels, &pixelSize)) {
                    bool hasAlpha = png_has_alpha(&pngHeader);

                    if (!framebuffer_draw_png(pixels, start_x, start_y, pngHeader.file.width, pngHeader.file.height, hasAlpha)) {
                        println("Failed to draw png");
                    }

                    kfree(pixels);
                } else {
                    println("Failed to decode png");
                }
            } else {
                println("Invalid PNG header");
            }
            break;
        }
        }
    } else {
        println("Couldnt open file");
    }

    kfree(imgBuffer);
}

void renderTexture(const char* path, uint32_t start_x, uint32_t start_y) {
    if (validate_bmp_header((const uint8_t*)path, iso9660_get_file_size(path), nullptr)) {
        renderTexture(ImageType::BMP_IMAGE, path, start_x, start_y);
    } else if (validate_png_header((const uint8_t*)path, iso9660_get_file_size(path), nullptr)) {
        renderTexture(ImageType::PNG_IMAGE, path, start_x, start_y);
    } else {
        println("Unsupported image format");
    }
}