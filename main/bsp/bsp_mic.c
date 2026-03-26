#include "bsp.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

static const char *TAG = "MIC";

// 拍手检测参数
#define CLAP_THRESHOLD    8000    // 幅度阈值（超过触发）
#define CLAP_COOLDOWN_MS  500     // 触发后冷却时间(ms)
#define CLAP_SAMPLE_RATE  16000   // INMP441 采样率

static bool s_mic_enabled = false;
static volatile bool s_clap_detected = false;
static volatile int64_t s_last_clap_time = 0;

// 拍手回调（由外部设置）
static void (*s_clap_callback)(void) = NULL;

void bsp_mic_set_clap_callback(void (*cb)(void))
{
    s_clap_callback = cb;
}

static bool s_task_started = false;

void bsp_mic_enable(bool en)
{
    if (en && !s_task_started) {
        s_task_started = true;
        xTaskCreate(bsp_mic_task, "mic_detect", 4096, NULL, 6, NULL);
        ESP_LOGI(TAG, "MIC task started");
    }
    s_mic_enabled = en;
    ESP_LOGI(TAG, "MIC %s", en ? "enabled" : "disabled");
}

// 从I2S RX读取一帧数据，计算幅度
static uint32_t read_mic_amplitude(void)
{
    extern i2s_chan_handle_t i2srx_handle;
    if (!i2srx_handle) return 0;

    int16_t samples[256];
    size_t bytes_read = 0;

    esp_err_t ret = i2s_channel_read(i2srx_handle, samples, sizeof(samples),
                                      &bytes_read, pdMS_TO_TICKS(10));
    if (ret != ESP_OK || bytes_read == 0) return 0;

    int count = bytes_read / sizeof(int16_t);
    uint32_t sum = 0;
    for (int i = 0; i < count; i++) {
        int32_t s = samples[i];
        sum += (s < 0 ? -s : s);
    }
    return sum / count;  // 平均绝对值幅度
}

void bsp_mic_task(void *pv)
{
    (void)pv;
    ESP_LOGI(TAG, "MIC clap detection task started");

    int64_t last_trigger = 0;

    while (1) {
        if (s_mic_enabled) {
            uint32_t amp = read_mic_amplitude();
            int64_t now = esp_timer_get_time() / 1000;

            if (amp > CLAP_THRESHOLD && (now - last_trigger) > CLAP_COOLDOWN_MS) {
                last_trigger = now;
                ESP_LOGI(TAG, "Clap detected! amp=%lu", (unsigned long)amp);
                if (s_clap_callback) {
                    s_clap_callback();
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(30));  // ~33Hz 检测频率
    }
}
