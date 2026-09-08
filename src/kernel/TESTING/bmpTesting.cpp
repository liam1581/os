#include "bmpTesting.hpp"
#include "drivers/video/imageRenderer.hpp"

extern "C" {
    #include "mem/mem.h"

    #include "drivers/files/image/bmp.h"
}


void bmpTestingMain() {
    //renderTexture(ImageType::BMP_IMAGE, "/images/ex_128x.bmp", 0, 150);
    renderTexture("/images/ex.png", 0, 150);
}