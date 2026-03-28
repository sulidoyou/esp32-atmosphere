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
    .rhythm = RHYTHM_SINGLE,
    .bpm = 120,
    .velocity = 70,
    .left_drum_ch = LEFT_DRUM_CH,
    .right_drum_ch = RIGHT_DRUM_CH,
    .running = false,
    .fire_limit = 0,
};

static uint8_t s_fires_fired = 0;  // 已触发次数（用于有限次数模式）
static uint8_t s_fire_limit = 0;   // fire_limit：0=无限，1=单击，2=双击
static drum_mode_source_t s_mode_source = DRUM_SOURCE_DEFAULT;  // 当前模式控制来源
static bool s_init_done = false;  // 初始化完成标志，防止music回调在init阶段覆盖

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

// 鼓点击中回调
static drum_hit_callback_t s_drum_hit_cb = NULL;

void bsp_drum_set_hit_callback(drum_hit_callback_t cb)
{
    s_drum_hit_cb = cb;
}

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
            
            // 调用鼓点击中回调 (用于串口屏节拍可视化)
            if (s_drum_hit_cb) {
                s_drum_hit_cb(req.drum);
            }
            
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
static void stop_timer(void);  // forward declaration
static void beat_timer_callback(TimerHandle_t t)
{
    (void)t;
    if (!s_drum.running) return;
    
    // 有限次数模式：已达到触发次数，停止
    if (s_fire_limit > 0 && s_fires_fired >= s_fire_limit) {
        ESP_LOGI(TAG, "DRUM STOP: fire_limit=%d fires=%d", s_fire_limit, s_fires_fired);
        s_drum.running = false;
        stop_timer();
        return;
    }

    const rhythm_pattern_t *rhythm = &RHYTHMS[s_current_rhythm];
    uint8_t beat = rhythm->pattern[s_beat_index % rhythm->beats];
    
    // beat值: 0=全休, 1=右电磁铁, 2=左电磁铁, 3=双面同时
    if (beat == 3) {
        // 拍掌：两边同时敲，分两次队列发送
        drum_hit_req_t req_l = {.drum = LEFT_DRUM_CH, .velocity = s_drum.velocity};
        drum_hit_req_t req_r = {.drum = RIGHT_DRUM_CH, .velocity = s_drum.velocity};
        xQueueSend(s_drum_queue, &req_l, 0);
        xQueueSend(s_drum_queue, &req_r, 0);
        s_fires_fired += 2;  // 算2次触发
    } else if (beat > 0) {
        drum_hit_req_t req = {
            .drum = (beat == 1) ? RIGHT_DRUM_CH : LEFT_DRUM_CH,
            .velocity = s_drum.velocity
        };
        xQueueSend(s_drum_queue, &req, 0);
        s_fires_fired++;
    }
    
    s_beat_index++;
    uint32_t beat_ms = BEAT_MS(s_drum.bpm) / 2;
    if (beat_ms < 50) beat_ms = 50;
    
    if (s_beat_timer == NULL) {
        s_beat_timer = xTimerCreateStatic("beat", pdMS_TO_TICKS(beat_ms), pdFALSE, NULL, beat_timer_callback, &s_beat_timer_buf);
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
    s_mode_source = DRUM_SOURCE_DEFAULT;
    s_init_done = false;  // 标记未完成，等待init结束时开启
    
    // 创建鼓点队列
    s_drum_queue = xQueueCreate(8, sizeof(drum_hit_req_t));
    if (s_drum_queue != NULL) {
        xTaskCreate(drum_task, "drum_proc", 4096, NULL, 3, NULL);
        ESP_LOGI(TAG, "Drum task started");
    }
    
    s_init_done = true;  // 初始化完成，允许music回调接管
    ESP_LOGI(TAG, "Drum init done, source=%d", s_mode_source);
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
    
    // NONE或MUSIC_SYNC模式 → 释放控制权，允许music接管
    if (mode == DRUM_MODE_NONE) {
        bsp_drum_stop();
        s_mode_source = DRUM_SOURCE_DEFAULT;
    } else if (mode == DRUM_MODE_MUSIC_SYNC) {
        // 用户选择"音乐同步" → 释放锁定，允许music模块控制
        s_mode_source = DRUM_SOURCE_DEFAULT;
    } else {
        // PRESET/MANUAL/MIC_SYNC → 用户设置，锁定模式不被music覆盖
        s_mode_source = DRUM_SOURCE_USER;
    }
    
    ESP_LOGI(TAG, "Drum mode: %d (source=%d)", mode, s_mode_source);
    s_drum.mode = mode;
}

drum_mode_source_t bsp_drum_get_source(void)
{
    return s_mode_source;
}

// 内部API：music模块调用，带来源标记
// 若当前已被用户锁定（SOURCE_USER），则不允许接管
// 返回true表示接管成功，false表示被用户锁定
bool bsp_drum_set_mode_auto(drum_mode_t mode, drum_mode_source_t src)
{
    // 初始化未完成时，music模块无法接管
    if (src == DRUM_SOURCE_MUSIC && !s_init_done) {
        ESP_LOGI(TAG, "Drum: music blocked (init not done)");
        return false;
    }

    if (src == DRUM_SOURCE_MUSIC && s_mode_source == DRUM_SOURCE_USER) {
        // 用户已锁定，音乐模块拒绝接管
        ESP_LOGI(TAG, "Drum: music blocked by user lock (mode=%d, source=%d)", s_drum.mode, s_mode_source);
        return false;
    }
    
    if (s_drum.mode == mode && s_mode_source == src) return true;
    
    if (mode == DRUM_MODE_NONE) {
        bsp_drum_stop();
    }
    
    ESP_LOGI(TAG, "Drum mode auto: %d (source=%d)", mode, src);
    s_mode_source = src;
    s_drum.mode = mode;
    return true;
}

// 内部API：Nextion/自动任务用，不影响SOURCE_USER锁定，不自动停止鼓
void bsp_drum_set_mode_quiet(drum_mode_t mode)
{
    if (s_drum.mode == mode) return;
    // 不调用stop，不改变source，不打印日志（安静）
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
    if (vel < 10) vel = 10;

    // BPM 安全限制：力度决定的通电时长不能超过 tick 间隔的 70%
    // 否则电磁铁来不及释放就被下一次 beat 打断
    // tick_ms = 60000 / BPM / 2，vel_ms = 50 + vel * 2.5
    uint16_t tick_ms = 60000 / s_drum.bpm / 2;
    // 使用int计算，避免高BPM时uint8_t负数溢出
    int max_vel = (tick_ms * 7 / 10 - 50) * 10 / 25;  // 70% 安全系数
    if (max_vel < 10) max_vel = 10;
    if (vel > max_vel) {
        ESP_LOGW(TAG, "Velocity %d clamped to %d (BPM=%d, tick=%dms, max_allowed=%dms)",
                 vel, max_vel, s_drum.bpm, tick_ms, 50 + max_vel * 25 / 10);
        vel = max_vel;
    }
    s_drum.velocity = vel;
}

void bsp_drum_set_rhythm(rhythm_type_t type)
{
    if (type > RHYTHM_ROCK) type = RHYTHM_ROCK;
    s_current_rhythm = type;
    s_drum.rhythm = type;  // 同步到info结构体
}

void bsp_drum_start(void)
{
    if (s_drum.mode == DRUM_MODE_NONE || s_drum.mode == DRUM_MODE_MANUAL) return;
    
    // 即使已经在跑，也要重置计时器（确保节奏变化后立即生效）
    s_drum.running = true;
    s_beat_index = 0;
    s_fires_fired = 0;
    ESP_LOGI(TAG, "Drum start: mode=%d, bpm=%d, fire_limit=%d", s_drum.mode, s_drum.bpm, s_drum.fire_limit);
    beat_timer_callback(NULL);
}

void bsp_drum_stop(void)
{
    if (!s_drum.running) return;
    ESP_LOGI(TAG, "Drum stop");
    s_drum.running = false;
    s_fires_fired = 0;
    s_fire_limit = 0;
    stop_timer();
}

void bsp_drum_set_fire_limit(uint8_t limit)
{
    s_fire_limit = limit;
    ESP_LOGI(TAG, "Drum fire_limit set to %d", limit);
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
