#pragma once

#include <stdint.h>
#include <stddef.h>

extern "C" {
    #include <stdint.h>
    #include "bool.h"
    #include "print.h"
    #include "mem/mem.h"
    #include "video/framebuffer.h"
    #include "drivers/storage/iso9660.h"
    #include "drivers/files/image/bmp.h"
}

enum class ImageType {
    BMP_IMAGE,
    PNG_IMAGE,
    JPG_IMAGE
};

template<typename T>
void renderTexture(ImageType imageType, const char* path, uint32_t start_x, uint32_t start_y) {   
    uint8_t* imgBuffer = (uint8_t*)kmalloc((size_t)iso9660_get_file_size(path));
    uint32_t outSize;

    T header;

    if (iso9660_read_file(path, imgBuffer, &outSize)) {
        switch (imageType) {
        case ImageType::BMP_IMAGE:
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
    } else {
        println("Couldnt open file");
    }

    kfree(imgBuffer);
}