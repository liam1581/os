#include "drivers/iso9660.h"
#include "drivers/atapi.h"

// ISO 9660 sector offsets
#define ISO9660_SYSTEM_AREA_SECTORS  16  // first 16 sectors are system area
#define ISO9660_PVD_SECTOR           16  // primary volume descriptor is at sector 16

// Volume descriptor types
#define ISO9660_VD_PRIMARY           0x01
#define ISO9660_VD_TERMINATOR        0xFF

// Directory record flags
#define ISO9660_FLAG_DIRECTORY       (1 << 1)

static uint8_t sector_buffer[SECTOR_SIZE];
static uint32_t root_lba;
static uint32_t root_size;
static bool initialized = false;

// Read a 16-bit little-endian value
static uint16_t read_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

// Read a 32-bit little-endian value
static uint32_t read_le32(const uint8_t* p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

// Copy and clean an ISO 9660 filename (strip version suffix like ";1")
static void copy_iso_name(char* dest, const uint8_t* src, uint8_t len) {
    uint8_t i;
    for (i = 0; i < len && i < ISO9660_MAX_FILENAME - 1; i++) {
        if (src[i] == ';') break;  // strip ";1" version suffix
        // lowercase
        char c = (char)src[i];
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        dest[i] = c;
    }
    dest[i] = '\0';
}

static bool strcmp_iso(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = ca - 'A' + 'a';
        if (cb >= 'A' && cb <= 'Z') cb = cb - 'A' + 'a';
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

bool iso9660_init() {
    if (!atapi_read_sector(ISO9660_PVD_SECTOR, sector_buffer)) {
        return false;
    }

    for (int i = 0; i < 8; i++) {
        uint8_t b = sector_buffer[i];
    }

    if (sector_buffer[1] != 'C' ||
        sector_buffer[2] != 'D' ||
        sector_buffer[3] != '0' ||
        sector_buffer[4] != '0' ||
        sector_buffer[5] != '1') {
        return false;
    }

    if (sector_buffer[0] != ISO9660_VD_PRIMARY) {
        return false;
    }

    const uint8_t* root = sector_buffer + 156;
    root_lba  = read_le32(root + 2);
    root_size = read_le32(root + 10);

    initialized = true;
    return true;
}

// Parse a directory sector into an ISO9660Dir, returns number of entries found
static uint32_t parse_directory_sector(const uint8_t* sector, struct ISO9660Dir* out) {
    uint32_t offset = 0;

    while (offset < SECTOR_SIZE) {
        uint8_t record_len = sector[offset];

        // record_len == 0 means end of directory entries in this sector
        if (record_len == 0) break;

        if (out->count >= ISO9660_MAX_CHILDREN) break;

        uint8_t  flags    = sector[offset + 25];
        uint32_t lba      = read_le32(sector + offset + 2);
        uint32_t size     = read_le32(sector + offset + 10);
        uint8_t  name_len = sector[offset + 32];
        const uint8_t* name = sector + offset + 33;

        // Skip dot and dotdot entries (name is 0x00 or 0x01)
        if (name_len == 1 && (name[0] == 0x00 || name[0] == 0x01)) {
            offset += record_len;
            continue;
        }

        struct ISO9660Entry* entry = &out->entries[out->count];
        copy_iso_name(entry->name, name, name_len);
        entry->is_directory = (flags & ISO9660_FLAG_DIRECTORY) != 0;
        entry->lba  = lba;
        entry->size = size;
        out->count++;

        offset += record_len;
    }

    return out->count;
}

bool iso9660_list_dir(const char* path, struct ISO9660Dir* out) {
    if (!initialized) return false;

    out->count = 0;

    uint32_t dir_lba  = root_lba;
    uint32_t dir_size = root_size;

    // Walk the path components e.g. "/boot/grub" → ["boot", "grub"]
    const char* p = path;
    if (*p == '/') p++;  // skip leading slash

    while (*p != '\0') {
        // Extract next path component
        char component[ISO9660_MAX_FILENAME];
        uint8_t ci = 0;
        while (*p != '/' && *p != '\0' && ci < ISO9660_MAX_FILENAME - 1) {
            component[ci++] = *p++;
        }
        component[ci] = '\0';
        if (*p == '/') p++;

        // Search current directory for this component
        bool found = false;
        uint32_t sectors = (dir_size + SECTOR_SIZE - 1) / SECTOR_SIZE;

        for (uint32_t s = 0; s < sectors && !found; s++) {
            if (!atapi_read_sector(dir_lba + s, sector_buffer)) return false;

            uint32_t offset = 0;
            while (offset < SECTOR_SIZE) {
                uint8_t record_len = sector_buffer[offset];
                if (record_len == 0) break;

                uint8_t  flags    = sector_buffer[offset + 25];
                uint32_t lba      = read_le32(sector_buffer + offset + 2);
                uint32_t size     = read_le32(sector_buffer + offset + 10);
                uint8_t  name_len = sector_buffer[offset + 32];
                const uint8_t* name = sector_buffer + offset + 33;

                if (name_len == 1 && (name[0] == 0x00 || name[0] == 0x01)) {
                    offset += record_len;
                    continue;
                }

                char entry_name[ISO9660_MAX_FILENAME];
                copy_iso_name(entry_name, name, name_len);

                if (strcmp_iso(entry_name, component)) {
                    if (!(flags & ISO9660_FLAG_DIRECTORY)) return false; // not a dir
                    dir_lba  = lba;
                    dir_size = size;
                    found = true;
                    break;
                }

                offset += record_len;
            }
        }

        if (!found) return false;
    }

    // Now list the target directory
    uint32_t sectors = (dir_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    for (uint32_t s = 0; s < sectors; s++) {
        if (!atapi_read_sector(dir_lba + s, sector_buffer)) return false;
        parse_directory_sector(sector_buffer, out);
    }

    return true;
}

bool iso9660_read_file(const char* path, uint8_t* buffer, uint32_t* out_size) {
    if (!initialized) return false;

    // Split path into directory and filename
    // e.g. "/boot/kernel.bin" → dir="/boot", file="kernel.bin"
    char dir_path[256];
    const char* filename = path;
    const char* last_slash = path;

    for (const char* p = path; *p; p++) {
        if (*p == '/') last_slash = p;
    }

    if (last_slash == path) {
        // File is in root
        dir_path[0] = '/';
        dir_path[1] = '\0';
        filename = path + 1;
    } else {
        uint32_t dir_len = (uint32_t)(last_slash - path);
        for (uint32_t i = 0; i < dir_len; i++) dir_path[i] = path[i];
        dir_path[dir_len] = '\0';
        filename = last_slash + 1;
    }

    // List the parent directory to find the file entry
    struct ISO9660Dir dir;
    if (!iso9660_list_dir(dir_path, &dir)) return false;

    for (uint32_t i = 0; i < dir.count; i++) {
        if (strcmp_iso(dir.entries[i].name, filename) && !dir.entries[i].is_directory) {
            uint32_t size    = dir.entries[i].size;
            uint32_t sectors = (size + SECTOR_SIZE - 1) / SECTOR_SIZE;

            for (uint32_t s = 0; s < sectors; s++) {
                if (!atapi_read_sector(dir.entries[i].lba + s, sector_buffer)) return false;

                uint32_t bytes = (s == sectors - 1) ? (size % SECTOR_SIZE) : SECTOR_SIZE;
                if (bytes == 0) bytes = SECTOR_SIZE;

                for (uint32_t b = 0; b < bytes; b++) {
                    buffer[s * SECTOR_SIZE + b] = sector_buffer[b];
                }
            }

            if (out_size) *out_size = size;
            return true;
        }
    }

    return false;
}