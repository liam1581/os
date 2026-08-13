#include "bmpTesting.hpp"

extern "C" {
    #include "print.h"
    #include "string.h"
    #include "video/framebuffer.h"

    #include "mem/mem.h"

    #include "debug.h"

    #include "drivers/files/bmp.h"
    #include "drivers/files/lfh.h"

    #include "drivers/iso9660.h"
}


void bmpTestingMain() {
    // uint8_t *imgBuffer = (uint8_t*)kmalloc(MEBIBYTE);
    // const char* imgPath = "/images/ex_512x.bmp";
    // uint32_t outSize;

    // BMPHeader header;

    // if (iso9660_read_file(imgPath, imgBuffer, &outSize)) {
    //     println("Read successfull!");
    //     if (validate_bmp_header(imgBuffer, outSize, &header)) {
    //         println("Valid BMP!");
    //         print("Width: ");
    //         print_uint64_dec(header.info.width);
    //         printc('\n');
    //         print("Height: ");
    //         print_uint64_dec(header.info.height);
    //         printc('\n');
    //         print("BPP: ");
    //         print_uint64_dec(header.info.bits_per_pixel);
    //         printc('\n');

    //         uint8_t *pixels = imgBuffer + header.file.pixel_offset;
            
    //         println("Drawing bmp at 0, 225");

    //         if (!framebuffer_draw_bmp(pixels, 0, 225, header.info.width, header.info.height)) {
    //             println("Failed to draw bmp");
    //         }

    //     } else {
    //         println("Invalid BMP");
    //     }
    // } else {
    //     println("Read failed");
    // }

    uint8_t *buffer = (uint8_t*)kmalloc(MEBIBYTE);
    const char* path = "/fonts/5x7.lfh";
    uint32_t outSize;

    LFHHeader header;
    if (iso9660_read_file(path, buffer, &outSize)) {
        println("Read successfull!");
        if (validate_lfh_header(buffer, outSize, &header)) {
            println("Valid LFH");
            print("Width: ");
            print_uint64_dec(header.file.width);
            printc('\n');
            print("Height: ");
            print_uint64_dec(header.file.height);
            printc('\n');
            print("Chars: ");
            print_uint64_dec(header.file.chars);
            printc('\n');

            uint8_t* glyphsOut = buffer + sizeof(LFHFileHeader);
            // uint8_t idk[665];
            // memcpy(idk, glyphsOut, 665));
            uint8_t glyphs[95][7];
            memcpy(glyphs, glyphsOut, 665);

            for (int i = 0; i<=7;i++) {
                print_uint64_hex(glyphs[1][i]);
                printc(' ');
            }
        } else {
            println("Invalid LFH");
        }
    } else {
        println("Read failed");
    }
}