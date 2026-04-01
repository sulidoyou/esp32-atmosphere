#include "bsp.h"
#include "esp_log.h"

static const char *TAG = "BSP";

// bsp_init: 板级初始化总入口
// 调用顺序很重要：电磁铁→I2C→I2S→CODEC→SD卡→音乐→灯光→按键→WiFi
// 前向声明（clap_trigger定义在bsp_init之后）
static void clap_trigger(void);

void bsp_init(void)
{
    ESP_LOGI(TAG, "Board init start");

    // 降低第三方组件日志噪音
    esp_log_level_set("file_iterator", ESP_LOG_WARN);
    esp_log_level_set("music_play",    ESP_LOG_WARN);
    esp_log_level_set("Adev_Codec",   ESP_LOG_WARN);
    esp_log_level_set("I2S_IF",       ESP_LOG_WARN);

    // ========== 电磁铁最先初始化，防止上电乱拍 ==========
    // bsp_magent_init()会设置所有GPIO为OUTPUT并关闭
    bsp_magent_init();
    ESP_LOGI(TAG, "Magnet: all OFF (anti-power-on glitch)");
    // ========== 电磁铁初始化完成 ==========

    bsp_i2c_init();           // I2C总线（ES8311 CODEC）
    bsp_i2s_init();           // I2S音频总线
    // bsp_pca9557_init();     // ❌ 硬件无PCA9557，仅74HC245（直接IO），注释掉避免I2C冲突
    bsp_es8311_init();        // ES8311音频编解码器

    esp_err_t tf_ret = tf_mount();  // SD卡挂载（音乐文件读取）
    if (tf_ret == ESP_OK) {
        mp3_palyer_init();          // 音乐播放器初始化
    } else {
        ESP_LOGE(TAG, "TF mount failed, skip mp3_palyer_init: %s", esp_err_to_name(tf_ret));
    }

    bsp_breathing_led_init(); // 呼吸灯PWM
    bsp_magent_init();         // 电磁铁GPIO（已初始化过，仅重新配置）
    bsp_drum_init();           // 鼓控制器初始化

    // 麦克风拍手检测
    extern void bsp_mic_enable(bool en);
    extern void bsp_mic_set_clap_callback(void (*cb)(void));
    bsp_mic_enable(true);
    bsp_mic_set_clap_callback(clap_trigger);

    bsp_key_init();           // 按键初始化

    bsp_wifi_init();          // WiFi连接（静态IP：192.168.0.247）
    bsp_nextion_init();       // Nextion串口屏 (GPIO43/44)

    ESP_LOGI(TAG, "Board init done");
}

// ---------------------------------------------------------------------------
// 以下为废弃代码（保留用于参考，bsp_init中未调用）
// 使用 mp3_palyer_init() 和 music_play/pause/next/prev 控制音乐
// ---------------------------------------------------------------------------

// 拍手触发：只在本机MIC_SYNC模式下敲鼓+灯光
static void clap_trigger(void)
{
    extern bool bsp_drum_is_mic_sync_mode(void);
    if (!bsp_drum_is_mic_sync_mode()) return;
    extern void bsp_drum_hit(uint8_t drum);
    extern void breathing_led_flash(uint16_t ms);
    bsp_drum_hit(0);  // LEFT_DRUM_CH
    breathing_led_flash(80);
}

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
