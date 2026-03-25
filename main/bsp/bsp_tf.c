#include "bsp.h"

#define TF_TAG "TF"
#define BSP_SD_CLK  47
#define BSP_SD_CMD  48
#define BSP_SD_D0   21

static sdmmc_card_t *_tf_card_info = NULL;

int tf_mounted(void)
{
    if (_tf_card_info == NULL) {
        return 0;
    }
    return 1;
}

esp_err_t tf_mount(void)
{
    if (_tf_card_info != NULL) {
        ESP_LOGW(TF_TAG, "Already mounted");
        return ESP_OK;
    }

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;  // 1线模式

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;
    slot_config.clk = BSP_SD_CLK;
    slot_config.cmd = BSP_SD_CMD;
    slot_config.d0 = BSP_SD_D0;
    slot_config.gpio_cd = GPIO_NUM_NC;  // 无卡检测引脚
    slot_config.gpio_wp = GPIO_NUM_NC;  // 无写保护引脚
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config,
                                             &mount_config, &_tf_card_info);
    if (ret != ESP_OK) {
        _tf_card_info = NULL;
        ESP_LOGW(TF_TAG, "Mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TF_TAG, "Mount success");
    tf_print_size_info();
    return ESP_OK;
}

esp_err_t tf_unmount(void)
{
    if (!tf_mounted()) {
        return ESP_OK;
    }

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(MOUNT_POINT, _tf_card_info);
    if (ret != ESP_OK) {
        ESP_LOGW(TF_TAG, "Unmount failed: %d", ret);
        return ret;
    }

    _tf_card_info = NULL;
    ESP_LOGI(TF_TAG, "Unmount success");
    return ESP_OK;
}

sdmmc_card_t *tf_info(void)
{
    return _tf_card_info;
}

void tf_print_size_info(void)
{
    if (!tf_mounted()) {
        ESP_LOGW(TF_TAG, "SD card not mounted");
        return;
    }

    uint64_t card_bytes = (uint64_t)_tf_card_info->csd.capacity
                        * _tf_card_info->csd.sector_size;

    uint64_t total_space = 0;
    uint64_t free_space = 0;
    esp_vfs_fat_info(MOUNT_POINT, &total_space, &free_space);

    uint64_t used_space = (total_space > free_space) ? (total_space - free_space) : 0;

    ESP_LOGI(TF_TAG, "=== SD Card Info ===");
    ESP_LOGI(TF_TAG, "Physical size: %.2f GB",
             (double)card_bytes / (1024 * 1024 * 1024));
    ESP_LOGI(TF_TAG, "FAT total:   %.2f GB",
             (double)total_space / (1024 * 1024 * 1024));
    ESP_LOGI(TF_TAG, "FAT used:    %.2f MB",
             (double)used_space / (1024 * 1024));
    ESP_LOGI(TF_TAG, "FAT free:    %.2f GB",
             (double)free_space / (1024 * 1024 * 1024));
}

// 返回值：1=存在, 0=不存在, -1=错误
int fatfs_file_exists(const char *filename)
{
    FILINFO fno;
    FRESULT fr = f_stat(filename, &fno);
    if (fr == FR_OK) return 1;
    if (fr == FR_NO_FILE) return 0;
    ESP_LOGW(TF_TAG, "f_stat error %d for %s", fr, filename);
    return -1;
}
