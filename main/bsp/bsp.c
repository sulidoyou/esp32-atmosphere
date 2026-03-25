#include "bsp.h"
#include "esp_log.h"

static const char *TAG = "BSP";

// bsp_init: 板级初始化总入口
// 调用顺序很重要：I2C→I2S→CODEC→SD卡→音乐→灯光→按键→WiFi
void bsp_init(void)
{
    ESP_LOGI(TAG, "Board init start");

    // 降低第三方组件日志噪音
    esp_log_level_set("file_iterator", ESP_LOG_WARN);
    esp_log_level_set("music_play",    ESP_LOG_WARN);
    esp_log_level_set("Adev_Codec",   ESP_LOG_WARN);
    esp_log_level_set("I2S_IF",       ESP_LOG_WARN);

    bsp_i2c_init();           // I2C总线（ES8311 CODEC）
    bsp_i2s_init();           // I2S音频总线
    bsp_pca9557_init();       // GPIO扩展器（PA功放控制）
    bsp_es8311_init();        // ES8311音频编解码器

    tf_mount();                // SD卡挂载（音乐文件读取）
    mp3_palyer_init();        // 音乐播放器初始化

    bsp_breathing_led_init(); // 呼吸灯PWM
    bsp_magent_init();         // 电磁铁GPIO

    bsp_key_init();           // 按键初始化
    bsp_wifi_init();          // WiFi连接（静态IP：192.168.0.200）

    ESP_LOGI(TAG, "Board init done");
}

// ---------------------------------------------------------------------------
// 以下为废弃代码（保留用于参考，bsp_init中未调用）
// 使用 mp3_palyer_init() 和 music_play/pause/next/prev 控制音乐
// ---------------------------------------------------------------------------

static uint16_t _RECORD_FILE_WRITE(const char *file_name, uint8_t *datas,
                                   uint16_t len, size_t max)
{
    static FILE *fd = NULL;
    static size_t acc = 0;
    if (fd == NULL) {
        fd = fopen(file_name, "wb");
        if (!fd) { printf("record: open failed\n"); return 0; }
        acc = 0;
    }
    uint16_t wlen = (uint16_t)fwrite(datas, 1, len, fd);
    acc += wlen;
    if (acc >= max) {
        fclose(fd); fd = NULL; acc = 0;
        printf("record done\n");
    }
    return wlen;
}

static uint16_t _MUSIC_FILE_READ(const char *file_name, uint8_t *datas, uint16_t size)
{
    static FILE *fd = NULL;
    if (fd == NULL) {
        fd = fopen(file_name, "rb");
        if (!fd) { printf("play: open failed\n"); return 0; }
    }
    uint16_t len = (uint16_t)fread(datas, 1, size, fd);
    if (len == 0) { fclose(fd); fd = NULL; printf("playback done\n"); }
    return len;
}

void music_play_task(void *args)
{
    (void)args;
    // 示例：录音回放
    // bsp_codec_play(_MUSIC_FILE_READ, MOUNT_POINT "/record.pcm", 15);
    vTaskDelete(NULL);
}

void music_play_start(void)
{
    xTaskCreate(music_play_task, "music_play_task", 10240, NULL, 10, NULL);
}
