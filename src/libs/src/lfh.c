#include "drivers/files/lfh.h"

#include <stdint.h>
#include <stddef.h>

#include "string.h"
#include "bool.h"

bool validate_lfh_header(const uint8_t* data, uint32_t file_size, LFHHeader* header) {
    if (data == NULL) // || header == NULL
        return false;
    
    if (file_size < sizeof(LFHHeader))
        return false;

    memcpy(&header->file, data, sizeof(LFHFileHeader));

    if (header->file.start != 0xFF)
        return false;
    
    if (header->file.signature != 0x0048464C)
        return false;
    
    if (header->file.width <= 0)
        return false;
    
    if (header->file.height <= 0)
        return false;
    
    if (header->file.chars <= 0)
        return false;
    
    if (header->file.end != 0xFF)
        return false;

    return true;
}