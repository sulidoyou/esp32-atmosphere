#include "bsp.h"
#define TF_TAG "TF"
/***************    SPIFFS文件系统 ↑  *********************/
/**********************************************************/
#define BSP_SD_CLK          (47)
#define BSP_SD_CMD          (48)
#define BSP_SD_D0           (21)
/**
 * @brief SD卡信息结构体指针
 *
 */
static sdmmc_card_t *_tf_card_info = NULL;

int tf_mounted()
{
    if (_tf_card_info == NULL) // 如果为空则没挂载上
    {
        ESP_LOGE(TF_TAG, "not mount");
        return 0;
    }
    return 1;
}
esp_err_t tf_mount(void)
{
   esp_err_t ret = ESP_OK;
    // 1. 配置 Locale 为中文（中国）UTF-8，覆盖所有类别
    setlocale(LC_ALL, "zh_CN.UTF-8");
    // 2.MMC 主机配置
    sdmmc_host_t host = SDMMC_HOST_DEFAULT(); // 获取SDMMC主机默认配置
    host.flags = SDMMC_HOST_FLAG_1BIT;        // 设置为1线模式
    // 3.MMC 接口配置
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT(); // 获取SDMMC槽默认配置
    slot_config.width = 1;                                         // 1线模式
    slot_config.clk = BSP_SD_CLK; 
    slot_config.cmd = BSP_SD_CMD;
    slot_config.d0 = BSP_SD_D0;
    slot_config.gpio_cd = GPIO_NUM_NC;                             // 不使用卡检测引脚（根据原理图）
    slot_config.gpio_wp = GPIO_NUM_NC;                             // 不使用写保护引脚（根据原理图）
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP; // 打开内部上拉电阻
    // 4.文件系统挂载配置
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,  // 挂载失败时不格式化
        .max_files = 5,                   // 最大同时打开文件数
        .allocation_unit_size = 16 * 1024 // 分配单元大小
    };
    // 挂载SD卡到文件系统
    ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &_tf_card_info);
    // 这里挂载可能出现已经挂载过的情况
    if (ret == ESP_OK)
    {
        ESP_LOGI(TF_TAG, "mount success"); // 挂载成功，标记为已挂载
        tf_print_size_info();
    }
    else
    {
        _tf_card_info = NULL;
        ESP_LOGW(TF_TAG, "mount failure %s", esp_err_to_name(ret));
    }
    return ret;
}


esp_err_t tf_unmount(void)
{
    esp_err_t ret = ESP_OK;
    if (!tf_mounted())
        return ret;
    ret = esp_vfs_fat_sdcard_unmount(MOUNT_POINT, _tf_card_info);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TF_TAG, "unmount success");
        _tf_card_info = NULL;
    }
    else
        ESP_LOGW(TF_TAG, "mount failure %d", ret);
    return ret;
}


sdmmc_card_t *tf_info()
{
    return _tf_card_info;
}

void tf_print_size_info()
{
    uint64_t card_total_bytes = 0;//TF 总大小
    uint64_t total_space = 0; //FS 总容量
    uint64_t free_space = 0;  //FS 剩余
    uint64_t used_space = 0;  //FS 使用
    if (!tf_mounted())
        return;
    // 1. 获取SD卡硬件总容量（物理容量）
    card_total_bytes = (uint64_t)_tf_card_info->csd.capacity * _tf_card_info->csd.sector_size;
    // 2. 获取FATFS分区的总空间和剩余空间
    esp_vfs_fat_info(MOUNT_POINT,&total_space,&free_space);
    used_space = total_space - free_space;
    // 打印信息 转换单位  Byte > GB
    ESP_LOGI(TF_TAG, "SD卡物理总容量: %.2f GB", (double)card_total_bytes / (1024 * 1024 * 1024));
    ESP_LOGI(TF_TAG, "FAT分区总空间: %.2f GB", (double)total_space / (1024 * 1024 * 1024));
    ESP_LOGI(TF_TAG, "已使用空间: %.6f GB", (double)used_space / (1024 * 1024 * 1024));
    ESP_LOGI(TF_TAG, "剩余空间: %.6f GB", (double)free_space / (1024 * 1024 * 1024));

    fatfs_file_exists(MOUNT_POINT "/gxfc.pcm");
}

int fatfs_file_exists(const char *filename)
{
    FILINFO fno;
    FRESULT fr;
    fr = f_stat(filename, &fno); // 尝试获取文件信息
    if (fr == FR_OK)
    {
        printf("file is yes\n"); 
        return 1; // 文件存在
    }
        
    else if (fr == FR_NO_FILE)
    {
        printf("file is no\n");
        return 0; // 文件不存在
    }
        
    else
    {
        printf("file error %d \n",fr);

    }
        return -1; // 其他错误（如路径错误、设备未挂载等），可根据需求处理
}
















