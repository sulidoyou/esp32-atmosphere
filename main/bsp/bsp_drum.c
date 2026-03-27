#include "bsp_drum.h"
#include "bsp_magent.h"
#include "bsp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "DRUM";

#define LEFT_DRUM_CH      0
#define RIGHT_DRUM_CH     1
#define BEAT_MS(bpm)      (60000 / (bpm))

static drum_info_t s_drum = {
    .mode = DRUM_MODE_NONE,
    .bpm = 120,
    .velocity = 70,
    .left_drum_ch = LEFT_DRUM_CH,
    .right_drum_ch = RIGHT_DRUM_CH,
    .running = false,
};

// 节奏序列 (0=左, 1=右, 2=停顿)
static const uint8_t PATTERN_SINGLE[] = {0, 2, 1, 2, 0, 2, 1, 2};
static const uint8_t PATTERN_DOUBLE[] = {0, 0, 2, 1, 2, 0, 0, 2, 1, 2};
static const uint8_t PATTERN_ROLL[] = {0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1};
static const uint8_t PATTERN_WALTZ[] = {0, 2, 1, 2, 0, 2, 1, 2, 0, 2, 1, 2};
static const uint8_t PATTERN_ROCK[] = {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1};

typedef struct {
    const char *name;
    const uint8_t *pattern;
    uint8_t beats;
    uint8_t divisor;
} rhythm_pattern_t;

static const rhythm_pattern_t RHYTHMS[] = {
    {"单击", PATTERN_SINGLE, sizeof(PATTERN_SINGLE), 4},
    {"双击", PATTERN_DOUBLE, sizeof(PATTERN_DOUBLE), 4},
    {"滚奏", PATTERN_ROLL, sizeof(PATTERN_ROLL), 4},
    {"华尔兹", PATTERN_WALTZ, sizeof(PATTERN_WALTZ), 3},
    {"摇滚", PATTERN_ROCK, sizeof(PATTERN_ROCK), 4},
};

static rhythm_type_t s_current_rhythm = RHYTHM_SINGLE;
static TimerHandle_t s_beat_timer = NULL;
static StaticTimer_t s_beat_timer_buf;
static int s_beat_index = 0;

// 鼓点队列（用于跨任务通信，避免定时器栈溢出）
static QueueHandle_t s_drum_queue = NULL;

// 鼓点请求结构
typedef struct {
    uint8_t drum;       // 0=左, 1=右
    uint8_t velocity;    // 力度 0-100
} drum_hit_req_t;

// 鼓点处理任务（运行在独立任务，栈足够大）
void drum_task(void *pv)
{
    (void)pv;
    drum_hit_req_t req;
    extern void breathing_led_flash(uint16_t flash_ms);
    
    while (1) {
        if (xQueueReceive(s_drum_queue, &req, portMAX_DELAY) == pdTRUE) {
            // 安全检查：只有非NONE模式才执行
            if (s_drum.mode == DRUM_MODE_NONE) continue;
            
            uint16_t ms = 50 + (req.velocity * 250 / 100);  // 50-300ms
            magent_fire_ch_with_ms(req.drum, ms);
            
            // LED闪光（只有预设模式才闪光）
            if (s_drum.mode == DRUM_MODE_PRESET) {
                uint16_t flash_ms = 30 + (req.velocity * 80 / 100);  // 30-110ms
                breathing_led_flash(flash_ms);
            }
        }
    }
}

static uint16_t velocity_to_ms(uint8_t vel)
{
    return 50 + (vel * 250 / 100);
}

// 定时器回调（极轻量，只发队列，不做实际工作）
static void beat_timer_callback(TimerHandle_t t)
{
    (void)t;
    if (!s_drum.running) return;
    
    const rhythm_pattern_t *rhythm = &RHYTHMS[s_current_rhythm];
    uint8_t beat = rhythm->pattern[s_beat_index % rhythm->beats];
    
    if (beat <= 1) {
        // 只发队列，不做任何IO操作
        drum_hit_req_t req = {
            .drum = (beat == 0) ? LEFT_DRUM_CH : RIGHT_DRUM_CH,
            .velocity = s_drum.velocity
        };
        xQueueSend(s_drum_queue, &req, 0);
    }
    
    s_beat_index++;
    uint32_t beat_ms = BEAT_MS(s_drum.bpm) / 2;
    if (beat_ms < 50) beat_ms = 50;
    
    if (s_beat_timer == NULL) {
        s_beat_timer = xTimerCreateStatic("beat", pdMS_TO_TICKS(beat_ms), pdTRUE, NULL, beat_timer_callback, &s_beat_timer_buf);
    } else {
        xTimerChangePeriod(s_beat_timer, pdMS_TO_TICKS(beat_ms), 0);
    }
    xTimerStart(s_beat_timer, 0);
}

static void stop_timer(void)
{
    if (s_beat_timer) {
        xTimerStop(s_beat_timer, 0);
    }
}

void bsp_drum_init(void)
{
    ESP_LOGI(TAG, "Drum init: CH1=GPIO17, CH2=GPIO18");
    s_drum.mode = DRUM_MODE_NONE;
    s_drum.running = false;
    
    // 创建鼓点队列
    s_drum_queue = xQueueCreate(8, sizeof(drum_hit_req_t));
    if (s_drum_queue != NULL) {
        xTaskCreate(drum_task, "drum_proc", 4096, NULL, 3, NULL);
        ESP_LOGI(TAG, "Drum task started");
    }
}

void bsp_drum_get_info(drum_info_t *info)
{
    if (info) {
        memcpy(info, &s_drum, sizeof(drum_info_t));
    }
}

void bsp_drum_set_mode(drum_mode_t mode)
{
    if (s_drum.mode == mode) return;
    
    // NONE模式时自动停止鼓
    if (mode == DRUM_MODE_NONE) {
        bsp_drum_stop();
    }
    
    ESP_LOGI(TAG, "Drum mode: %d", mode);
    s_drum.mode = mode;
}

void bsp_drum_set_bpm(uint8_t bpm)
{
    if (bpm < 60) bpm = 60;
    if (bpm > 240) bpm = 240;
    s_drum.bpm = bpm;
}

void bsp_drum_set_velocity(uint8_t vel)
{
    if (vel > 100) vel = 100;
    s_drum.velocity = vel;
}

void bsp_drum_set_rhythm(rhythm_type_t type)
{
    if (type > RHYTHM_ROCK) type = RHYTHM_ROCK;
    s_current_rhythm = type;
}

void bsp_drum_start(void)
{
    if (s_drum.running) return;
    if (s_drum.mode == DRUM_MODE_NONE || s_drum.mode == DRUM_MODE_MANUAL) return;
    
    ESP_LOGI(TAG, "Drum start: mode=%d, bpm=%d", s_drum.mode, s_drum.bpm);
    s_drum.running = true;
    s_beat_index = 0;
    beat_timer_callback(NULL);
}

void bsp_drum_stop(void)
{
    if (!s_drum.running) return;
    ESP_LOGI(TAG, "Drum stop");
    s_drum.running = false;
    stop_timer();
}

void bsp_drum_hit(uint8_t drum)
{
    drum_hit_req_t req = {
        .drum = (drum == 0) ? LEFT_DRUM_CH : RIGHT_DRUM_CH,
        .velocity = s_drum.velocity
    };
    xQueueSend(s_drum_queue, &req, 0);
}

bool bsp_drum_is_mic_sync_mode(void)
{
    return s_drum.mode == DRUM_MODE_MIC_SYNC;
}
