/*
 * 氛围系统 ESP32-S3 主程序
 * 功能: 音乐播放 + 氛围灯 + 4路电磁铁 + WiFi + HTTP控制接口
 */

#include "bsp.h"
#include "bsp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "Atmosphere";

// ============ 网络状态LED任务 ============
// GPIO3 网络LED：WiFi断开→熄灭，WiFi连接→快闪5Hz
void network_led_task(void *pv)
{
    (void)pv;
    ESP_LOGI(TAG, "Network LED task started");

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_3),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    static bool last_wifi_ok = false;
    bool led_on = false;

    while (1) {
        bsp_wifi_poll();

        bool wifi_ok = WiFi_connected();

        if (wifi_ok) {
            led_on = !led_on;
            gpio_set_level(GPIO_NUM_3, led_on ? 1 : 0);
        } else {
            led_on = false;
            gpio_set_level(GPIO_NUM_3, 0);  // 断开时熄灭
        }

        // WiFi状态变化时打印
        if (wifi_ok != last_wifi_ok) {
            last_wifi_ok = wifi_ok;
            if (wifi_ok) {
                ESP_LOGI(TAG, "WiFi connected | IP: %s", WiFi_getIP());
            } else {
                ESP_LOGI(TAG, "WiFi disconnected");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));  // 5Hz闪烁（100ms翻转）
    }
}

// ============ 氛围灯模式自动切换任务 ============
void atmosphere_mode_task(void *pv)
{
    (void)pv;
    ESP_LOGI(TAG, "Auto mode switch task started");

    while (1) {
        // 每10秒切换：呼吸↔彩虹
        static uint8_t mode = BREATHING_LED_MODE_BREATH;
        mode = (mode == BREATHING_LED_MODE_BREATH)
             ? BREATHING_LED_MODE_RAINBOW
             : BREATHING_LED_MODE_BREATH;
        breathing_led_set_mode(mode);
        ESP_LOGI(TAG, "Auto mode: %s", mode == BREATHING_LED_MODE_BREATH ? "breath" : "rainbow");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

// ============ 音乐播放状态任务（调试用）============
void music_status_task(void *pv)
{
    (void)pv;
    ESP_LOGI(TAG, "Music status task started");

    while (1) {
        if (tf_mounted()) {
            ESP_LOGI(TAG, "SD card OK, music playing");
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// ============ 启动横幅 ============
static void print_banner(void)
{
    printf("\n");
    printf("========================================\n");
    printf("   氛围系统 ESP32-S3 v1.2\n");
    printf("   Free Luck Amusement Co., Ltd.\n");
    printf("========================================\n");
    printf("  GPIO46: Breathing LED (LEDC PWM)\n");
    printf("  GPIO17/18/16/15: Magnet CH1-4\n");
    printf("  GPIO3:  Network LED\n");
    printf("  SD:     CLK=47 CMD=48 D0=21\n");
    printf("  I2C:    SCL=2 SDA=1 (ES8311/PCA9557)\n");
    printf("  I2S:    MCLK=38 BCLK=14 WS=13\n");
    printf("  DOUT=45 DIN=12\n");
    printf("  WiFi:   freeluck6 @ 192.168.0.200\n");
    printf("  HTTP:    http://192.168.0.200\n");
    printf("========================================\n\n");
}

// ============ 主程序入口 ============
void app_main(void)
{
    print_banner();
    ESP_LOGI(TAG, "Initializing BSP...");
    bsp_init();
    ESP_LOGI(TAG, "BSP init done");

    if (tf_mounted()) {
        ESP_LOGI(TAG, "SD card detected");
    } else {
        ESP_LOGE(TAG, "SD card NOT detected!");
    }

    // 启动各任务
    xTaskCreate(network_led_task,      "net_led",      4096, NULL, 3, NULL);
    xTaskCreate(atmosphere_mode_task,   "atmo_mode",    4096, NULL, 4, NULL);
    xTaskCreate(music_status_task,      "music_status", 4096, NULL, 2, NULL);

    // HTTP服务器（网页控制面板）
    ESP_LOGI(TAG, "Starting HTTP server...");
    bsp_http_server_start();
    ESP_LOGI(TAG, "HTTP server started");

    ESP_LOGI(TAG, "=== Atmosphere System Ready ===");
    ESP_LOGI(TAG, "Web UI: http://192.168.0.200");
}
