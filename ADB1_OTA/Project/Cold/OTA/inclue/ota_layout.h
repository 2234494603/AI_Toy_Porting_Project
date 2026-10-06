#ifndef OTA_LAYOUT_H
#define OTA_LAYOUT_H

#include <stdint.h>

#define OTA_FLASH_BASE_ADDRESS       (0x08000000UL)
#define OTA_BOOTLOADER_SIZE          (0x00004000UL)
#define OTA_APP_BASE_ADDRESS         (OTA_FLASH_BASE_ADDRESS + OTA_BOOTLOADER_SIZE)
#define OTA_METADATA_ADDRESS         (0x0800FC00UL)
#define OTA_APP_MAX_SIZE             (OTA_METADATA_ADDRESS - OTA_APP_BASE_ADDRESS)
#define OTA_FLASH_PAGE_SIZE          (0x00000400UL)
#define OTA_SRAM_BASE_ADDRESS        (0x20000000UL)
#define OTA_SRAM_END_ADDRESS         (0x20005000UL)
#define OTA_METADATA_MAGIC           (0x4F544131UL)
#define OTA_METADATA_FORMAT_VERSION  (1UL)
#define OTA_STATE_PENDING            (0xFFFFFFFEUL)
#define OTA_STATE_DOWNLOADING        (0xFFFFFFFCUL)
#define OTA_STATE_VERIFIED           (0xFFFFFFF8UL)
#define OTA_STATE_CONFIRMED          (0xFFFFFFF0UL)

typedef struct {
    uint32_t magic;
    uint32_t format_version;
    uint32_t state;
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t version_code;
    uint32_t reserved[2];
} ota_metadata_t;

#endif
