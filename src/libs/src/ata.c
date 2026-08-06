#include "drivers/ata.h"
#include "x86_64/port.h"

#include "debug.h"

#define ATA_PRIMARY_DATA         0x1F0
#define ATA_PRIMARY_ERROR        0x1F1
#define ATA_PRIMARY_SECTOR_COUNT 0x1F2
#define ATA_PRIMARY_LBA_LOW      0x1F3
#define ATA_PRIMARY_LBA_MID      0x1F4
#define ATA_PRIMARY_LBA_HIGH     0x1F5
#define ATA_PRIMARY_DRIVE_SELECT 0x1F6
#define ATA_PRIMARY_STATUS       0x1F7
#define ATA_PRIMARY_COMMAND      0x1F7
#define ATA_PRIMARY_ALT_STATUS   0x3F6

#define ATA_STATUS_ERR  (1 << 0)
#define ATA_STATUS_DRQ  (1 << 3)
#define ATA_STATUS_SRV  (1 << 4)
#define ATA_STATUS_DF   (1 << 5)
#define ATA_STATUS_RDY  (1 << 6)
#define ATA_STATUS_BSY  (1 << 7)

#define ATA_CMD_READ_SECTORS  0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_IDENTIFY      0xEC
#define ATA_CMD_FLUSH         0xE7

#define ATA_TIMEOUT 100000

static bool ata_wait_bsy() {
    for (uint32_t i = 0; i < ATA_TIMEOUT; i++) {
        if (!(port_inb(ATA_PRIMARY_STATUS) & ATA_STATUS_BSY)) return true;
    }
    serial_write("ata::ata_wait_bsy::timed out");
    return false;
}

static bool ata_wait_drq() {
    for (uint32_t i = 0; i < ATA_TIMEOUT; i++) {
        if (port_inb(ATA_PRIMARY_STATUS) & ATA_STATUS_DRQ) return true;
    }
    serial_write("ata::ata_wait_drw::timed out");
    return false;
}

static bool ata_check_error() {
    uint8_t status = port_inb(ATA_PRIMARY_STATUS);
    return (status & ATA_STATUS_ERR) || (status & ATA_STATUS_DF);
}

static void ata_select_drive(uint32_t lba) {
    // Select master drive, LBA mode, bits 24-27 of LBA in low nibble
    port_outb(ATA_PRIMARY_DRIVE_SELECT, 0xE0 | ((lba >> 24) & 0x0F));
}

bool ata_init() {
    port_outb(ATA_PRIMARY_DRIVE_SELECT, 0xA0);
    for (int i = 0; i < 15; i++) port_wait();

    uint8_t status = port_inb(ATA_PRIMARY_STATUS);
    if (status == 0xFF || status == 0x00) return false;

    port_outb(ATA_PRIMARY_SECTOR_COUNT, 0);
    port_outb(ATA_PRIMARY_LBA_LOW,  0);
    port_outb(ATA_PRIMARY_LBA_MID,  0);
    port_outb(ATA_PRIMARY_LBA_HIGH, 0);
    port_outb(ATA_PRIMARY_COMMAND, ATA_CMD_IDENTIFY);

    status = port_inb(ATA_PRIMARY_STATUS);
    if (status == 0) return false;

    if (!ata_wait_bsy()) return false;

    if (port_inb(ATA_PRIMARY_LBA_MID) != 0 ||
        port_inb(ATA_PRIMARY_LBA_HIGH) != 0) return false;

    if (!ata_wait_drq()) return false;
    if (ata_check_error()) return false;

    for (int i = 0; i < 256; i++) port_inw(ATA_PRIMARY_DATA);

    return true;
}

bool ata_read_sector(uint32_t lba, uint8_t* buffer) {
    if (!ata_wait_bsy()) return false;
    ata_select_drive(lba);
    port_outb(ATA_PRIMARY_SECTOR_COUNT, 1);
    port_outb(ATA_PRIMARY_LBA_LOW,  (uint8_t)(lba));
    port_outb(ATA_PRIMARY_LBA_MID,  (uint8_t)(lba >> 8));
    port_outb(ATA_PRIMARY_LBA_HIGH, (uint8_t)(lba >> 16));
    port_outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_SECTORS);

    if (!ata_wait_bsy()) return false;
    if (!ata_wait_drq()) return false;
    if (ata_check_error()) return false;

    for (int i = 0; i < 256; i++) {
        uint16_t word = port_inw(ATA_PRIMARY_DATA);
        buffer[i * 2]     = (uint8_t)(word & 0xFF);
        buffer[i * 2 + 1] = (uint8_t)(word >> 8);
    }
    return true;
}

bool ata_write_sector(uint32_t lba, const uint8_t* buffer) {
    if (!ata_wait_bsy()) return false;
    ata_select_drive(lba);
    port_outb(ATA_PRIMARY_SECTOR_COUNT, 1);
    port_outb(ATA_PRIMARY_LBA_LOW,  (uint8_t)(lba));
    port_outb(ATA_PRIMARY_LBA_MID,  (uint8_t)(lba >> 8));
    port_outb(ATA_PRIMARY_LBA_HIGH, (uint8_t)(lba >> 16));
    port_outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_SECTORS);

    if (!ata_wait_bsy()) return false;
    if (!ata_wait_drq()) return false;
    if (ata_check_error()) return false;

    for (int i = 0; i < 256; i++) {
        uint16_t word = (uint16_t)buffer[i * 2] | ((uint16_t)buffer[i * 2 + 1] << 8);
        port_outw(ATA_PRIMARY_DATA, word);
    }

    port_outb(ATA_PRIMARY_COMMAND, ATA_CMD_FLUSH);
    if (!ata_wait_bsy()) return false;

    return true;
}