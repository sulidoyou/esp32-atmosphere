/*
 * 氛围系统 ESP32-S3 主程序
 * 功能: 音乐播放 + 氛围灯 + 4路电磁铁 + WiFi + HTTP控制接口
 */

#include "bsp.h"
#include "bsp_http_server.h"
#include "bsp_nextion.h"
#include "bsp_drum.h"
#include "bsp_breathing_led.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"

static const char *TAG = "Atmosphere";

// ============ 网络状态LED任务 ============
// GPIO3 网络LED：WiFi断开→熄灭，WiFi连接→快闪5Hz
void network_led_task(void *pv)
{
    (void)pv;
    esp_task_wdt_add(NULL);  // 订阅TWDT
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
    esp_task_wdt_add(NULL);  // Bug fix: 订阅TWDT
    ESP_LOGI(TAG, "Auto mode switch task started");

    while (1) {
        // 每10秒切换：呼吸↔彩虹
        static uint8_t mode = BREATHING_LED_MODE_BREATH;
        mode = (mode == BREATHING_LED_MODE_BREATH)
             ? BREATHING_LED_MODE_RAINBOW
             : BREATHING_LED_MODE_BREATH;
        breathing_led_set_mode(mode);
        // ESP_LOGI(TAG, "Auto mode: %s", mode == BREATHING_LED_MODE_BREATH ? "breath" : "rainbow");
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
            // ESP_LOGI(TAG, "SD card OK, music playing");
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// ============ Nextion串口屏显示任务 ============
// 鼓点回调适配器 (drum -> beat+bpm)
static void _drum_hit_for_nextion(uint8_t drum)
{
    drum_info_t info;
    bsp_drum_get_info(&info);
    bsp_nextion_update_beat(drum, info.bpm);
}

void nextion_display_task(void *pv)
{
    (void)pv;
    esp_task_wdt_add(NULL);  // Bug fix: 订阅TWDT
    ESP_LOGI(TAG, "Nextion display task started");
    
    // 等待BSP完全初始化
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 注册鼓点回调 (用于节拍可视化)
    bsp_drum_set_hit_callback(_drum_hit_for_nextion);
    
    // 模式名称表 (与bsp_breathing_led.h一致)
    const char *mode_names[] = {"Idle", "Breath", "Rainbow"};
    const char *scene_names[] = {"Idle", "Game1", "Game2", "Party", "Relax"};
    
    while (1) {
        // 读取鼓点状态
        drum_info_t drum_info;
        bsp_drum_get_info(&drum_info);
        
        // 更新系统状态 (呼吸灯模式)
        uint8_t led_mode = breathing_led_get_mode();
        bsp_nextion_update_system(drum_info.running, led_mode);
        
        // 更新网络状态
        bool wifi_ok = WiFi_connected();
        const char *ip = WiFi_getIP();
        bsp_nextion_update_network(wifi_ok, ip);
        
        // 更新场景 (基于鼓点模式)
        const char *scene_name = "Idle";
        uint8_t scene_id = 0;
        switch (drum_info.mode) {
            case DRUM_MODE_PRESET:   scene_name = "Game1"; scene_id = 1; break;
            case DRUM_MODE_MANUAL:   scene_name = "Game2"; scene_id = 2; break;
            case DRUM_MODE_MIC_SYNC: scene_name = "Party"; scene_id = 3; break;
            default:                 scene_name = "Idle";  scene_id = 0; break;
        }
        bsp_nextion_update_scene(scene_name, scene_id);
        
        // 更新BPM
        bsp_nextion_update_bpm(drum_info.bpm);
        
        // 节拍可视化由鼓点任务回调触发，这里只更新运行状态
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ============ 启动横幅 ============
static void print_banner(void)
{
    printf("\n");
    printf("========================================\n");
    printf("   氛围系统 ESP32-S3 " APP_VERSION_STR "\n");
    printf("   Free Luck Amusement Co., Ltd.\n");
    printf("========================================\n");
    printf("  GPIO46: Breathing LED (LEDC PWM)\n");
    printf("  GPIO17/18/16/15: Magnet CH1-4\n");
    printf("  GPIO3:  Network LED\n");
    printf("  GPIO43/44: Nextion LCD (115200)\n");
    printf("  SD:     CLK=47 CMD=48 D0=21\n");
    printf("  I2C:    SCL=2 SDA=1 (ES8311/PCA9557)\n");
    printf("  I2S:    MCLK=38 BCLK=14 WS=13\n");
    printf("  DOUT=45 DIN=12\n");
    printf("  WiFi:   freeluck6 @ 192.168.0.247\n");
    printf("  HTTP:    http://192.168.0.247\n");
    printf("========================================\n\n");
}

// ============ 主程序入口 ============
void app_main(void)
{
    print_banner();
    ESP_LOGI(TAG, "Initializing BSP...");
    bsp_init();
    ESP_LOGI(TAG, "BSP init done");

    // ============ TWDT 看门狗初始化（Bug fix: 防止系统死机）============
    // 配置：超时10秒，订阅当前任务和CPU IDLE任务
    // 注意：menuconfig中需启用CONFIG_ESP_TASK_WDT_EN=y
    // 如果CONFIG_ESP_TASK_WDT_EN未启用，esp_task_wdt_init会返回ESP_ERR_INVALID_STATE
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 10000,
        .idle_core_mask = 0,  // 不自动订阅IDLE任务，由各任务自行订阅
    };
    esp_err_t twdt_err = esp_task_wdt_init(&twdt_config);
    if (twdt_err == ESP_OK) {
        ESP_LOGI(TAG, "TWDT initialized: timeout=10s");
        // 订阅主任务（防止主任务死循环）
        esp_task_wdt_add(NULL);
    } else if (twdt_err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "TWDT already initialized by menuconfig (ESP_ERR_INVALID_STATE)");
        // 如果menuconfig已启用TWDT，仍然添加主任务
        esp_task_wdt_add(NULL);
    } else {
        ESP_LOGE(TAG, "TWDT init failed: %s", esp_err_to_name(twdt_err));
    }

    if (tf_mounted()) {
        ESP_LOGI(TAG, "SD card detected");
    } else {
        ESP_LOGE(TAG, "SD card NOT detected!");
    }

    // FreeRTOS多任务音乐播放系统初始化
    // 在bsp_init()（包含mp3_palyer_init()）之后初始化
    // 支持音效中断（祝贺音<50ms响应）和背景音乐并行播放
    ESP_LOGI(TAG, "Initializing FreeRTOS music system...");
    if (bsp_music_freertos_init() == ESP_OK) {
        ESP_LOGI(TAG, "FreeRTOS music system init OK");
    } else {
        ESP_LOGE(TAG, "FreeRTOS music system init FAILED!");
    }

    // 启动各任务（均检查返回值，任务创建失败应报警）
    BaseType_t x;
    x = xTaskCreate(network_led_task,      "net_led",      4096, NULL, 3, NULL);
    if (x != pdPASS) { ESP_LOGE(TAG, "FAIL create net_led (ret=%d)", x); }
    x = xTaskCreate(atmosphere_mode_task,  "atmo_mode",    4096, NULL, 4, NULL);
    if (x != pdPASS) { ESP_LOGE(TAG, "FAIL create atmo_mode (ret=%d)", x); }
    x = xTaskCreate(music_status_task,     "music_status", 4096, NULL, 2, NULL);
    if (x != pdPASS) { ESP_LOGE(TAG, "FAIL create music_status (ret=%d)", x); }
    x = xTaskCreate(nextion_display_task,  "nextion_disp", 4096, NULL, 2, NULL);
    if (x != pdPASS) { ESP_LOGE(TAG, "FAIL create nextion_disp (ret=%d)", x); }

    // HTTP服务器（网页控制面板）
    ESP_LOGI(TAG, "Starting HTTP server...");
    bsp_http_server_start();
    ESP_LOGI(TAG, "HTTP server started");

    ESP_LOGI(TAG, "=== Atmosphere System Ready ===");
    ESP_LOGI(TAG, "Web UI: http://192.168.0.247");
}
