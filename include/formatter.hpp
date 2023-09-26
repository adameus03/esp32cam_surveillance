#include "ff.h"
#include "vfs_fat_internal.h"

/**
 * Usage:
 *   // See https://github.com/espressif/esp-idf/blob/b63ec47238fd6aa6eaa59f7ad3942cbdff5fcc1f/examples/storage/sd_card/sdmmc/main/sd_card_example_main.c#L75
 *   esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
 *   format_sdcard(card);
 *   // proceed without remounting
 */
esp_err_t format_sdcard(sdmmc_card_t *card);