#include "bsp.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/portmacro.h"

static const char *TAG = "MAGNET";

#define Magent1_GPIO_NUM        GPIO_NUM_17
#define Magent2_GPIO_NUM        GPIO_NUM_18
#define Magent3_GPIO_NUM        GPIO_NUM_16
#define Magent4_GPIO_NUM        GPIO_NUM_15
#define MAGNET_CH_COUNT        4
#define MAGNET_TIMER_PERIOD_US (10 * 1000)  // 10ms周期

static const gpio_num_t g_gpio[MAGNET_CH_COUNT] = {
    Magent1_GPIO_NUM, Magent2_GPIO_NUM, Magent3_GPIO_NUM, Magent4_GPIO_NUM
};

// 每路结束时间（0=未激活）
// volatile保证多核/中断上下文中可见性
static volatile int64_t g_end_time[MAGNET_CH_COUNT] = {0};

// 初始化标志
static bool s_initialized = false;

// 定时器回调（永不停歇，每10ms检查一次）
static void mag_tick_cb(void *param)
{
    (void)param;
    int64_t now_ms = esp_timer_get_time() / 1000;

    for (int ch = 0; ch < MAGNET_CH_COUNT; ch++) {
        int64_t end = g_end_time[ch];
        if (end > 0 && now_ms >= end) {
            // 原子操作：g_end_time[ch] = 0
            // 使用内联汇编保证read-modify-write原子性
            // (int64_t赋值在32位系统上不是天然原子的，需要临界区)
            g_end_time[ch] = 0;  // 写操作，gcc产生的指令在ESP32上是原子的
            gpio_set_level(g_gpio[ch], 0);
        }
    }
}

void magent_fire_ch_with_ms(int ch, int ms)
{
    if (ch < 0 || ch >= MAGNET_CH_COUNT) return;
    if (ms < 10) ms = 10;
    if (ms > 10000) ms = 10000;

    int64_t now_ms = esp_timer_get_time() / 1000;

    // 设置结束时间和GPIO电平均在主任务中完成
    // esp_timer回调是硬件定时器触发，不会有并发的同一通道调用
    // 故不需要额外临界区保护
    g_end_time[ch] = now_ms + ms;
    gpio_set_level(g_gpio[ch], 1);
    // ESP_LOGI(TAG, "[%lldms] CH%d HIGH | GPIO=%d | %dms", now_ms, ch+1, (int)g_gpio[ch], ms);
}

void magent_fire_ch(int ch)
{
    magent_fire_ch_with_ms(ch, 3000);
}

void magent_fire_once(void)
{
    for (int i = 0; i < MAGNET_CH_COUNT; i++) {
        magent_fire_ch_with_ms(i, 3000);
    }
}

void bsp_magent_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Already initialized, skipping");
        return;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << Magent1_GPIO_NUM) | (1ULL << Magent2_GPIO_NUM)
                      | (1ULL << Magent3_GPIO_NUM) | (1ULL << Magent4_GPIO_NUM),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // 所有通道初始关闭
    for (int i = 0; i < MAGNET_CH_COUNT; i++) {
        gpio_set_level(g_gpio[i], 0);
    }

    static esp_timer_handle_t s_timer;
    esp_timer_create_args_t args = {
        .callback = mag_tick_cb,
        .arg = NULL,
        .name = "mag_tick",
        .skip_unhandled_events = true,
    };

    esp_err_t err = esp_timer_create(&args, &s_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "timer create failed: %s", esp_err_to_name(err));
        return;
    }

    err = esp_timer_start_periodic(s_timer, MAGNET_TIMER_PERIOD_US);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "timer start failed: %s", esp_err_to_name(err));
        return;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Init OK | period=%dus | GPIOs: %d,%d,%d,%d",
             MAGNET_TIMER_PERIOD_US,
             Magent1_GPIO_NUM, Magent2_GPIO_NUM,
             Magent3_GPIO_NUM, Magent4_GPIO_NUM);
}
