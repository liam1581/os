#include "drivers/fat32.h"
#include "drivers/ata.h"

// ─── BPB (BIOS Parameter Block) ───────────────────────────────────────────────

typedef struct {
    uint8_t  jump[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;   // 0 for FAT32
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;        // 0 for FAT32
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    // FAT32 extended
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_sig;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
} __attribute__((packed)) FAT32BPB;

// ─── Directory entry (32 bytes) ────────────────────────────────────────────────

typedef struct {
    uint8_t  name[8];
    uint8_t  ext[3];
    uint8_t  attr;
    uint8_t  reserved;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t cluster_low;
    uint32_t size;
} __attribute__((packed)) FAT32DirEntry;

// ─── LFN entry ─────────────────────────────────────────────────────────────────

typedef struct {
    uint8_t  order;
    uint16_t name1[5];
    uint8_t  attr;
    uint8_t  type;
    uint8_t  checksum;
    uint16_t name2[6];
    uint16_t cluster;
    uint16_t name3[2];
} __attribute__((packed)) FAT32LFNEntry;

// ─── Globals ───────────────────────────────────────────────────────────────────

static FAT32BPB bpb;
static uint32_t fat_start_lba;
static uint32_t data_start_lba;
static uint32_t sectors_per_cluster;
static uint32_t root_cluster;
static bool initialized = false;

static uint8_t sector_buf[ATA_SECTOR_SIZE];
static uint8_t fat_buf[ATA_SECTOR_SIZE];
static uint32_t fat_buf_lba = 0xFFFFFFFF;

// ─── Helpers ───────────────────────────────────────────────────────────────────

static uint32_t cluster_to_lba(uint32_t cluster) {
    return data_start_lba + (cluster - 2) * sectors_per_cluster;
}

// Read a FAT sector, cached
static bool fat_read(uint32_t lba) {
    if (fat_buf_lba == lba) return true;
    if (!ata_read_sector(lba, fat_buf)) return false;
    fat_buf_lba = lba;
    return true;
}

static uint32_t fat_get_next_cluster(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start_lba + (fat_offset / ATA_SECTOR_SIZE);
    uint32_t fat_index  = (fat_offset % ATA_SECTOR_SIZE) / 4;
    if (!fat_read(fat_sector)) return 0x0FFFFFFF;
    uint32_t val = ((uint32_t*)fat_buf)[fat_index] & 0x0FFFFFFF;
    return val;
}

static bool fat_set_next_cluster(uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start_lba + (fat_offset / ATA_SECTOR_SIZE);
    uint32_t fat_index  = (fat_offset % ATA_SECTOR_SIZE) / 4;
    if (!fat_read(fat_sector)) return false;
    ((uint32_t*)fat_buf)[fat_index] = (((uint32_t*)fat_buf)[fat_index] & 0xF0000000) | (value & 0x0FFFFFFF);
    fat_buf_lba = 0xFFFFFFFF; // invalidate cache so it gets re-read
    return ata_write_sector(fat_sector, fat_buf);
}

// Find a free cluster
static uint32_t fat_alloc_cluster() {
    uint32_t total_clusters = (bpb.total_sectors_32 - data_start_lba) / sectors_per_cluster + 2;
    for (uint32_t c = 2; c < total_clusters; c++) {
        uint32_t next = fat_get_next_cluster(c);
        if (next == 0x00000000) {
            fat_set_next_cluster(c, 0x0FFFFFFF); // mark end of chain
            return c;
        }
    }
    return 0; // disk full
}

// Zero out a cluster
static bool cluster_zero(uint32_t cluster) {
    uint8_t zero[ATA_SECTOR_SIZE];
    for (int i = 0; i < ATA_SECTOR_SIZE; i++) zero[i] = 0;
    uint32_t lba = cluster_to_lba(cluster);
    for (uint32_t s = 0; s < sectors_per_cluster; s++) {
        if (!ata_write_sector(lba + s, zero)) return false;
    }
    return true;
}

static bool str_eq_nocase(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a;
        char cb = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static uint32_t str_len(const char* s) {
    uint32_t i = 0;
    while (s[i]) i++;
    return i;
}

static void str_copy(char* dest, const char* src) {
    uint32_t i = 0;
    while (src[i]) { dest[i] = src[i]; i++; }
    dest[i] = '\0';
}

// Parse LFN entries into a name string
static void parse_lfn(FAT32LFNEntry* lfn, char* out) {
    uint16_t chars[13];
    chars[0]  = lfn->name1[0]; chars[1]  = lfn->name1[1];
    chars[2]  = lfn->name1[2]; chars[3]  = lfn->name1[3];
    chars[4]  = lfn->name1[4];
    chars[5]  = lfn->name2[0]; chars[6]  = lfn->name2[1];
    chars[7]  = lfn->name2[2]; chars[8]  = lfn->name2[3];
    chars[9]  = lfn->name2[4]; chars[10] = lfn->name2[5];
    chars[11] = lfn->name3[0]; chars[12] = lfn->name3[1];

    uint8_t seq = (lfn->order & 0x1F) - 1;
    for (int i = 0; i < 13; i++) {
        out[seq * 13 + i] = (char)(chars[i] & 0xFF);
    }
}

// Read a full 8.3 name into a string
static void parse_83_name(FAT32DirEntry* e, char* out) {
    uint32_t i = 0, j = 0;
    // name part (strip trailing spaces)
    uint32_t name_end = 8;
    while (name_end > 0 && e->name[name_end - 1] == ' ') name_end--;
    for (j = 0; j < name_end; j++) {
        char c = (char)e->name[j];
        out[i++] = (c >= 'A' && c <= 'Z') ? c + 32 : c;
    }
    // ext part
    uint32_t ext_end = 3;
    while (ext_end > 0 && e->ext[ext_end - 1] == ' ') ext_end--;
    if (ext_end > 0) {
        out[i++] = '.';
        for (j = 0; j < ext_end; j++) {
            char c = (char)e->ext[j];
            out[i++] = (c >= 'A' && c <= 'Z') ? c + 32 : c;
        }
    }
    out[i] = '\0';
}

// Write an 8.3 name from a long name (truncates, uppercase)
static void make_83_name(const char* name, uint8_t* out_name, uint8_t* out_ext) {
    for (int i = 0; i < 8; i++) out_name[i] = ' ';
    for (int i = 0; i < 3; i++) out_ext[i]  = ' ';

    // Find dot
    int dot = -1;
    for (int i = 0; name[i]; i++) if (name[i] == '.') dot = i;

    int i = 0, j = 0;
    while (name[i] && name[i] != '.' && j < 8) {
        char c = name[i];
        out_name[j++] = (c >= 'a' && c <= 'z') ? c - 32 : c;
        i++;
    }
    if (dot >= 0) {
        i = dot + 1; j = 0;
        while (name[i] && j < 3) {
            char c = name[i];
            out_ext[j++] = (c >= 'a' && c <= 'z') ? c - 32 : c;
            i++;
        }
    }
}

// ─── Init ──────────────────────────────────────────────────────────────────────

bool fat32_init() {
    if (!ata_read_sector(0, sector_buf)) return false;

    // Copy BPB from boot sector
    for (uint32_t i = 0; i < sizeof(FAT32BPB); i++) {
        ((uint8_t*)&bpb)[i] = sector_buf[i];
    }

    // Validate
    if (bpb.bytes_per_sector != 512)  return false;
    if (bpb.fat_size_16 != 0)         return false; // not FAT32

    fat_start_lba    = bpb.reserved_sectors;
    data_start_lba   = fat_start_lba + (bpb.num_fats * bpb.fat_size_32);
    sectors_per_cluster = bpb.sectors_per_cluster;
    root_cluster     = bpb.root_cluster;
    initialized      = true;
    return true;
}

// ─── Directory reading ─────────────────────────────────────────────────────────

// Read all entries from a directory cluster chain into out
static bool read_dir_cluster(uint32_t cluster, struct FAT32Dir* out) {
    char lfn_buf[FAT32_MAX_FILENAME];
    for (int i = 0; i < FAT32_MAX_FILENAME; i++) lfn_buf[i] = '\0';
    bool has_lfn = false;

    while (cluster < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(cluster);
        for (uint32_t s = 0; s < sectors_per_cluster; s++) {
            if (!ata_read_sector(lba + s, sector_buf)) return false;
            FAT32DirEntry* entries = (FAT32DirEntry*)sector_buf;

            for (uint32_t i = 0; i < ATA_SECTOR_SIZE / 32; i++) {
                FAT32DirEntry* e = &entries[i];
                if (e->name[0] == 0x00) return true; // end of dir
                if (e->name[0] == 0xE5) { has_lfn = false; continue; } // deleted

                if (e->attr == FAT32_ATTR_LFN) {
                    parse_lfn((FAT32LFNEntry*)e, lfn_buf);
                    has_lfn = true;
                    continue;
                }

                if (e->attr & FAT32_ATTR_VOLUME_ID) { has_lfn = false; continue; }
                if (e->name[0] == '.') { has_lfn = false; continue; }
                if (out->count >= FAT32_MAX_CHILDREN) return true;

                struct FAT32Entry* entry = &out->entries[out->count];
                if (has_lfn) {
                    str_copy(entry->name, lfn_buf);
                    for (int j = 0; j < FAT32_MAX_FILENAME; j++) lfn_buf[j] = '\0';
                    has_lfn = false;
                } else {
                    parse_83_name(e, entry->name);
                }

                entry->is_directory = (e->attr & FAT32_ATTR_DIRECTORY) != 0;
                entry->cluster      = ((uint32_t)e->cluster_high << 16) | e->cluster_low;
                entry->size         = e->size;
                out->count++;
            }
        }
        cluster = fat_get_next_cluster(cluster);
    }
    return true;
}

// Resolve a path to a cluster number (returns root_cluster for "/")
static bool resolve_path(const char* path, uint32_t* out_cluster, bool* out_is_dir, uint32_t* out_size) {
    uint32_t cluster = root_cluster;
    *out_is_dir = true;
    *out_size   = 0;

    const char* p = path;
    if (*p == '/') p++;
    if (*p == '\0') { *out_cluster = cluster; return true; }

    while (*p != '\0') {
        char component[FAT32_MAX_FILENAME];
        uint32_t ci = 0;
        while (*p != '/' && *p != '\0') component[ci++] = *p++;
        component[ci] = '\0';
        if (*p == '/') p++;

        struct FAT32Dir dir;
        dir.count = 0;
        if (!read_dir_cluster(cluster, &dir)) return false;

        bool found = false;
        for (uint32_t i = 0; i < dir.count; i++) {
            if (str_eq_nocase(dir.entries[i].name, component)) {
                cluster     = dir.entries[i].cluster;
                *out_is_dir = dir.entries[i].is_directory;
                *out_size   = dir.entries[i].size;
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    *out_cluster = cluster;
    return true;
}

bool fat32_list_dir(const char* path, struct FAT32Dir* out) {
    if (!initialized) return false;
    out->count = 0;
    uint32_t cluster; bool is_dir; uint32_t size;
    if (!resolve_path(path, &cluster, &is_dir, &size)) return false;
    if (!is_dir) return false;
    return read_dir_cluster(cluster, out);
}

bool fat32_read_file(const char* path, uint8_t* buffer, uint32_t* out_size) {
    if (!initialized) return false;
    uint32_t cluster; bool is_dir; uint32_t size;
    if (!resolve_path(path, &cluster, &is_dir, &size)) return false;
    if (is_dir) return false;

    uint32_t bytes_read = 0;
    uint32_t cluster_size = sectors_per_cluster * ATA_SECTOR_SIZE;

    while (cluster < 0x0FFFFFF8 && bytes_read < size) {
        uint32_t lba = cluster_to_lba(cluster);
        for (uint32_t s = 0; s < sectors_per_cluster && bytes_read < size; s++) {
            if (!ata_read_sector(lba + s, sector_buf)) return false;
            uint32_t to_copy = size - bytes_read;
            if (to_copy > ATA_SECTOR_SIZE) to_copy = ATA_SECTOR_SIZE;
            for (uint32_t i = 0; i < to_copy; i++) buffer[bytes_read + i] = sector_buf[i];
            bytes_read += to_copy;
        }
        cluster = fat_get_next_cluster(cluster);
    }

    if (out_size) *out_size = size;
    return true;
}

// ─── Write helpers ─────────────────────────────────────────────────────────────

// Find the parent dir cluster and filename from a full path
static bool split_path(const char* path, uint32_t* parent_cluster, char* filename) {
    // Find last slash
    int last_slash = -1;
    for (int i = 0; path[i]; i++) if (path[i] == '/') last_slash = i;

    if (last_slash <= 0) {
        // File is in root
        *parent_cluster = root_cluster;
        str_copy(filename, path[0] == '/' ? path + 1 : path);
    } else {
        char parent_path[256];
        for (int i = 0; i < last_slash; i++) parent_path[i] = path[i];
        parent_path[last_slash] = '\0';
        str_copy(filename, path + last_slash + 1);
        uint32_t c; bool is_dir; uint32_t size;
        if (!resolve_path(parent_path, &c, &is_dir, &size)) return false;
        if (!is_dir) return false;
        *parent_cluster = c;
    }
    return true;
}

// Write a new 8.3 directory entry into a directory cluster chain
static bool write_dir_entry(uint32_t dir_cluster, FAT32DirEntry* new_entry) {
    uint32_t cluster = dir_cluster;
    while (cluster < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(cluster);
        for (uint32_t s = 0; s < sectors_per_cluster; s++) {
            if (!ata_read_sector(lba + s, sector_buf)) return false;
            FAT32DirEntry* entries = (FAT32DirEntry*)sector_buf;
            for (uint32_t i = 0; i < ATA_SECTOR_SIZE / 32; i++) {
                if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
                    entries[i] = *new_entry;
                    return ata_write_sector(lba + s, sector_buf);
                }
            }
        }
        uint32_t next = fat_get_next_cluster(cluster);
        if (next >= 0x0FFFFFF8) {
            // Extend directory with a new cluster
            uint32_t new_cluster = fat_alloc_cluster();
            if (!new_cluster) return false;
            fat_set_next_cluster(cluster, new_cluster);
            cluster_zero(new_cluster);
            cluster = new_cluster;
        } else {
            cluster = next;
        }
    }
    return false;
}

// Find and update an existing directory entry by cluster
static bool update_dir_entry(uint32_t dir_cluster, uint32_t file_cluster, FAT32DirEntry* updated) {
    uint32_t cluster = dir_cluster;
    while (cluster < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(cluster);
        for (uint32_t s = 0; s < sectors_per_cluster; s++) {
            if (!ata_read_sector(lba + s, sector_buf)) return false;
            FAT32DirEntry* entries = (FAT32DirEntry*)sector_buf;
            for (uint32_t i = 0; i < ATA_SECTOR_SIZE / 32; i++) {
                uint32_t ec = ((uint32_t)entries[i].cluster_high << 16) | entries[i].cluster_low;
                if (ec == file_cluster && entries[i].name[0] != 0xE5) {
                    entries[i] = *updated;
                    return ata_write_sector(lba + s, sector_buf);
                }
            }
        }
        cluster = fat_get_next_cluster(cluster);
    }
    return false;
}

// Free an entire cluster chain
static void fat_free_chain(uint32_t cluster) {
    while (cluster < 0x0FFFFFF8 && cluster >= 2) {
        uint32_t next = fat_get_next_cluster(cluster);
        fat_set_next_cluster(cluster, 0x00000000);
        cluster = next;
    }
}

// ─── Public write API ──────────────────────────────────────────────────────────

bool fat32_create_file(const char* path) {
    if (!initialized) return false;
    uint32_t parent; char filename[FAT32_MAX_FILENAME];
    if (!split_path(path, &parent, filename)) return false;

    uint32_t cluster = fat_alloc_cluster();
    if (!cluster) return false;
    cluster_zero(cluster);

    FAT32DirEntry entry = {0};
    make_83_name(filename, entry.name, entry.ext);
    entry.attr         = FAT32_ATTR_ARCHIVE;
    entry.cluster_high = (uint16_t)(cluster >> 16);
    entry.cluster_low  = (uint16_t)(cluster & 0xFFFF);
    entry.size         = 0;

    return write_dir_entry(parent, &entry);
}

bool fat32_create_dir(const char* path) {
    if (!initialized) return false;
    uint32_t parent; char dirname[FAT32_MAX_FILENAME];
    if (!split_path(path, &parent, dirname)) return false;

    uint32_t cluster = fat_alloc_cluster();
    if (!cluster) return false;
    cluster_zero(cluster);

    // Write . and .. entries
    uint32_t lba = cluster_to_lba(cluster);
    if (!ata_read_sector(lba, sector_buf)) return false;
    FAT32DirEntry* entries = (FAT32DirEntry*)sector_buf;

    // . entry
    for (int i = 0; i < 8; i++) entries[0].name[i] = ' ';
    for (int i = 0; i < 3; i++) entries[0].ext[i]  = ' ';
    entries[0].name[0]     = '.';
    entries[0].attr        = FAT32_ATTR_DIRECTORY;
    entries[0].cluster_high = (uint16_t)(cluster >> 16);
    entries[0].cluster_low  = (uint16_t)(cluster & 0xFFFF);
    entries[0].size        = 0;

    // .. entry
    for (int i = 0; i < 8; i++) entries[1].name[i] = ' ';
    for (int i = 0; i < 3; i++) entries[1].ext[i]  = ' ';
    entries[1].name[0]     = '.';
    entries[1].name[1]     = '.';
    entries[1].attr        = FAT32_ATTR_DIRECTORY;
    entries[1].cluster_high = (uint16_t)(parent >> 16);
    entries[1].cluster_low  = (uint16_t)(parent & 0xFFFF);
    entries[1].size        = 0;

    if (!ata_write_sector(lba, sector_buf)) return false;

    // Write entry in parent
    FAT32DirEntry entry = {0};
    make_83_name(dirname, entry.name, entry.ext);
    entry.attr         = FAT32_ATTR_DIRECTORY;
    entry.cluster_high = (uint16_t)(cluster >> 16);
    entry.cluster_low  = (uint16_t)(cluster & 0xFFFF);
    entry.size         = 0;

    return write_dir_entry(parent, &entry);
}

bool fat32_write_file(const char* path, const uint8_t* buffer, uint32_t size) {
    if (!initialized) return false;
    uint32_t parent; char filename[FAT32_MAX_FILENAME];
    if (!split_path(path, &parent, filename)) return false;

    uint32_t cluster; bool is_dir; uint32_t old_size;
    if (!resolve_path(path, &cluster, &is_dir, &old_size)) return false;
    if (is_dir) return false;

    // Free old chain and allocate fresh one
    fat_free_chain(cluster);
    uint32_t first_cluster = 0, prev_cluster = 0;
    uint32_t bytes_written = 0;
    uint32_t cluster_size = sectors_per_cluster * ATA_SECTOR_SIZE;

    do {
        uint32_t new_cluster = fat_alloc_cluster();
        if (!new_cluster) return false;
        if (!first_cluster) first_cluster = new_cluster;
        if (prev_cluster)   fat_set_next_cluster(prev_cluster, new_cluster);

        uint32_t lba = cluster_to_lba(new_cluster);
        for (uint32_t s = 0; s < sectors_per_cluster; s++) {
            uint8_t tmp[ATA_SECTOR_SIZE] = {0};
            uint32_t offset = bytes_written + s * ATA_SECTOR_SIZE;
            uint32_t to_copy = (offset < size) ? (size - offset) : 0;
            if (to_copy > ATA_SECTOR_SIZE) to_copy = ATA_SECTOR_SIZE;
            for (uint32_t i = 0; i < to_copy; i++) tmp[i] = buffer[offset + i];
            if (!ata_write_sector(lba + s, tmp)) return false;
        }

        bytes_written += cluster_size;
        prev_cluster   = new_cluster;
    } while (bytes_written < size);

    // Update directory entry with new cluster and size
    FAT32DirEntry updated = {0};
    make_83_name(filename, updated.name, updated.ext);
    updated.attr         = FAT32_ATTR_ARCHIVE;
    updated.cluster_high = (uint16_t)(first_cluster >> 16);
    updated.cluster_low  = (uint16_t)(first_cluster & 0xFFFF);
    updated.size         = size;

    return update_dir_entry(parent, cluster, &updated);
}

bool fat32_rename(const char* path, const char* new_name) {
    if (!initialized) return false;
    uint32_t parent; char filename[FAT32_MAX_FILENAME];
    if (!split_path(path, &parent, filename)) return false;

    uint32_t cluster; bool is_dir; uint32_t size;
    if (!resolve_path(path, &cluster, &is_dir, &size)) return false;

    FAT32DirEntry updated = {0};
    make_83_name(new_name, updated.name, updated.ext);
    updated.attr         = is_dir ? FAT32_ATTR_DIRECTORY : FAT32_ATTR_ARCHIVE;
    updated.cluster_high = (uint16_t)(cluster >> 16);
    updated.cluster_low  = (uint16_t)(cluster & 0xFFFF);
    updated.size         = size;

    return update_dir_entry(parent, cluster, &updated);
}

bool fat32_copy_file(const char* src, const char* dest) {
    if (!initialized) return false;

    uint32_t cluster; bool is_dir; uint32_t size;
    if (!resolve_path(src, &cluster, &is_dir, &size)) return false;
    if (is_dir) return false;

    // Read source into a temporary buffer
    // Note: for large files you'd want chunked copying
    uint8_t* buf = (uint8_t*)0x600000; // temp buffer above LSE load address
    if (!fat32_read_file(src, buf, &size)) return false;

    if (!fat32_create_file(dest)) return false;
    return fat32_write_file(dest, buf, size);
}

bool fat32_move_file(const char* src, const char* dest) {
    if (!initialized) return false;
    if (!fat32_copy_file(src, dest)) return false;
    return fat32_delete_file(src);
}

bool fat32_delete_file(const char* path) {
    if (!initialized) return false;
    uint32_t parent; char filename[FAT32_MAX_FILENAME];
    if (!split_path(path, &parent, filename)) return false;

    uint32_t cluster; bool is_dir; uint32_t size;
    if (!resolve_path(path, &cluster, &is_dir, &size)) return false;
    if (is_dir) return false;

    // Free cluster chain
    fat_free_chain(cluster);

    // Mark directory entry as deleted (0xE5)
    uint32_t dc = parent;
    while (dc < 0x0FFFFFF8) {
        uint32_t lba = cluster_to_lba(dc);
        for (uint32_t s = 0; s < sectors_per_cluster; s++) {
            if (!ata_read_sector(lba + s, sector_buf)) return false;
            FAT32DirEntry* entries = (FAT32DirEntry*)sector_buf;
            for (uint32_t i = 0; i < ATA_SECTOR_SIZE / 32; i++) {
                uint32_t ec = ((uint32_t)entries[i].cluster_high << 16) | entries[i].cluster_low;
                if (ec == cluster && entries[i].name[0] != 0xE5) {
                    entries[i].name[0] = 0xE5;
                    return ata_write_sector(lba + s, sector_buf);
                }
            }
        }
        dc = fat_get_next_cluster(dc);
    }
    return false;
}