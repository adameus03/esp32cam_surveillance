#include "ff.h"
#include "vfs_fat_internal.h"

/**
 * Usage:
 *   // See https://github.com/espressif/esp-idf/blob/b63ec47238fd6aa6eaa59f7ad3942cbdff5fcc1f/examples/storage/sd_card/sdmmc/main/sd_card_example_main.c#L75
 *   esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
 *   format_sdcard(card);
 *   // proceed without remounting
 */
esp_err_t format_sdcard(sdmmc_card_t *card) {
	char drv[3] = {'0', ':', 0};
    const size_t workbuf_size = 4096;
    void* workbuf = NULL;
    esp_err_t err = ESP_OK;
    ESP_LOGW("sdcard", "Formatting the SD card");

    size_t allocation_unit_size = 16 * 1024;

    workbuf = ff_memalloc(workbuf_size);
    if (workbuf == NULL) {
        return ESP_ERR_NO_MEM;
    }

    size_t alloc_unit_size = esp_vfs_fat_get_allocation_unit_size(
                card->csd.sector_size,
                allocation_unit_size);

    FRESULT res = f_mkfs(drv, FM_ANY, alloc_unit_size, workbuf, workbuf_size);
    if (res != FR_OK) {
        err = ESP_FAIL;
        ESP_LOGE("sdcard", "f_mkfs failed (%d)", res);
    }

    free(workbuf);

    ESP_LOGI("sdcard", "Successfully formatted the SD card");

    return err;
}