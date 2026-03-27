#include "bsp.h"
#include "driver/ledc.h"

#define BREATHING_LED_GPIO     GPIO_NUM_46
#define BREATHING_LED_CHANNEL  LEDC_CHANNEL_0

// 呼吸灯参数
#define BREATH_STEP_BRIGHT   4     // 每步亮度变化量
#define BREATH_MIN_BRIGHT   0
#define BREATH_MAX_BRIGHT   255
#define BREATH_FADE_MS      50     // 渐变时间(ms)
#define BREATH_PAUSE_MS     300    // 到达极值后暂停(ms)
#define BREATH_STEP_MS      20     // 主循环周期(ms)

// 彩虹灯参数
#define RAINBOW_HUE_STEP    3      // 每步色相变化
#define RAINBOW_BRIGHT_MAX  200
#define RAINBOW_BRIGHT_MIN  30
#define RAINBOW_BRIGHT_STEP 2
#define RAINBOW_FADE_MS     30
#define RAINBOW_PAUSE_MS    0      // 无暂停，更流畅

static const char *TAG = "BreathingLED";

static uint8_t s_mode = 0;       // 0=呼吸, 1=彩虹, 2=鼓点闪
static uint16_t s_hue = 0;        // 色相 0~359
static uint8_t  s_brightness = 128;
static int8_t   s_direction = 1;  // 1=渐亮, -1=渐暗
static uint16_t s_flash_ms = 0;    // 鼓点闪光剩余时间(ms)，0=无闪光

// HSV → RGB 转换（来源：ESP-IDF LEDC例程）
static void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v,
                       uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (s == 0) {
        *r = *g = *b = v;
        return;
    }

    uint16_t region = h / 60;
    uint16_t remainder = (uint16_t)((h - region * 60) * 255 / 60);
    uint8_t p = (uint16_t)(v * (255 - s)) / 255;
    uint8_t q = (uint16_t)(v * (255 - (uint16_t)s * remainder / 255)) / 255;
    uint8_t t = (uint16_t)(v * (255 - (uint16_t)s * (255 - remainder) / 255)) / 255;

    switch (region) {
        case 0:  *r = v; *g = t; *b = p; break;
        case 1:  *r = q; *g = v; *b = p; break;
        case 2:  *r = p; *g = v; *b = t; break;
        case 3:  *r = p; *g = q; *b = v; break;
        case 4:  *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

void breathing_led_task(void *pvParameters)
{
    (void)pvParameters;

    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num  = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz    = 1000,
        .clk_cfg   = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_cfg);

    ledc_channel_config_t ch_cfg = {
        .gpio_num   = BREATHING_LED_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = BREATHING_LED_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&ch_cfg);

    // fade服务安装（不需要卸载）
    ledc_fade_func_install(0);

    const uint32_t max_duty = (1 << LEDC_TIMER_8_BIT) - 1;  // 255
    ESP_LOGI(TAG, "LED init: GPIO%02d, max_duty=%lu",
             BREATHING_LED_GPIO, (unsigned long)max_duty);

    while (1) {
        if (s_mode == 2) {
            // --- 鼓点闪光模式：全亮直到 flash_ms 耗尽 ---
            if (s_flash_ms > 0) {
                uint32_t flash_duty = (1 << LEDC_TIMER_8_BIT) - 1;  // 255
                ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, BREATHING_LED_CHANNEL,
                                         flash_duty, 5);  // 5ms快速拉满
                ledc_fade_start(LEDC_LOW_SPEED_MODE, BREATHING_LED_CHANNEL, LEDC_FADE_NO_WAIT);
                vTaskDelay(pdMS_TO_TICKS(s_flash_ms > 50 ? 50 : s_flash_ms));
                if (s_flash_ms >= 50) {
                    s_flash_ms -= 50;
                } else {
                    s_flash_ms = 0;
                }
                if (s_flash_ms == 0) {
                    // 闪光结束，恢复呼吸模式
                    s_mode = 0;
                    // ESP_LOGI(TAG, "LED flash done, restore breath mode");
                }
            } else {
                s_mode = 0;
            }
        } else if (s_mode == 0) {
            // --- 呼吸模式 ---
            uint32_t target = s_brightness;
            ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, BREATHING_LED_CHANNEL,
                                   target, BREATH_FADE_MS);
            ledc_fade_start(LEDC_LOW_SPEED_MODE, BREATHING_LED_CHANNEL,
                            LEDC_FADE_NO_WAIT);

            if (s_direction == 1) {
                s_brightness += BREATH_STEP_BRIGHT;
                if (s_brightness >= BREATH_MAX_BRIGHT) {
                    s_brightness = BREATH_MAX_BRIGHT;
                    s_direction = -1;
                    vTaskDelay(pdMS_TO_TICKS(BREATH_PAUSE_MS));
                }
            } else {
                s_brightness -= BREATH_STEP_BRIGHT;
                if (s_brightness <= BREATH_MIN_BRIGHT) {
                    s_brightness = BREATH_MIN_BRIGHT;
                    s_direction = 1;
                    vTaskDelay(pdMS_TO_TICKS(BREATH_PAUSE_MS));
                }
            }
        } else {
            // --- 彩虹呼吸模式 ---
            // 当前GPIO46是单通道，只能控制亮度，
            // 色相变化记录在日志中供调试，亮度按彩虹呼吸规律变化
            uint8_t r, g, b;
            hsv_to_rgb(s_hue, 255, s_brightness, &r, &g, &b);

            // 单GPIO限流：用绿色通道近似亮度感
            uint32_t duty = (uint32_t)g;
            if (duty > max_duty) duty = max_duty;

            ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, BREATHING_LED_CHANNEL,
                                   duty, RAINBOW_FADE_MS);
            ledc_fade_start(LEDC_LOW_SPEED_MODE, BREATHING_LED_CHANNEL,
                            LEDC_FADE_NO_WAIT);

            // 色相旋转
            s_hue = (s_hue + RAINBOW_HUE_STEP) % 360;

            // 亮度呼吸
            if (s_direction == 1) {
                s_brightness += RAINBOW_BRIGHT_STEP;
                if (s_brightness >= RAINBOW_BRIGHT_MAX) {
                    s_brightness = RAINBOW_BRIGHT_MAX;
                    s_direction = -1;
                }
            } else {
                if (s_brightness <= RAINBOW_BRIGHT_MIN) {
                    s_brightness = RAINBOW_BRIGHT_MIN;
                    s_direction = 1;
                }
                s_brightness -= RAINBOW_BRIGHT_STEP;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(BREATH_STEP_MS));
    }
}

void breathing_led_set_mode(uint8_t mode)
{
    // 允许0/1，其他值忽略
    if (mode > 1) return;
    s_mode = mode;
    // ESP_LOGI(TAG, "Mode set to: %s", mode == 0 ? "breath" : "rainbow");
}

// 鼓点闪光：调用后LED立即全亮，持续ms后自动恢复
void breathing_led_flash(uint16_t ms)
{
    s_flash_ms = ms;
    s_mode = 2;  // flash mode
    // ESP_LOGI(TAG, "LED flash: %ums", ms);
}

uint8_t breathing_led_get_mode(void)
{
    return s_mode;
}

void bsp_breathing_led_init(void)
{
    xTaskCreate(breathing_led_task, "breathing_led", 4096, NULL, 4, NULL);
}
