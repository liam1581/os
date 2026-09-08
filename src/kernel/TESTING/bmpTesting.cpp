#include "bmpTesting.hpp"
#include "drivers/video/imageRenderer.hpp"

extern "C" {
    #include "mem/mem.h"

    #include "drivers/files/image/bmp.h"
    #include "timer.h"
}


void bmpTestingMain() {
    renderTexture(ImageType::PNG_IMAGE, "/images/ex_ss.png", 0, 150);
}