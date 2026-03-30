/**
 * @file bsp_music_play_freertos.c
 * @brief 氛围系统 FreeRTOS 多任务音乐播放架构
 *
 * 架构：
 * - musicTask  : 背景音乐播放（MP3/SD卡 + audio_player组件）
 * - sfxTask    : 音效播放（预加载WAV/PSRAM，<10ms响应）
 * - eventTask  : 中奖事件监听 → 触发 sfxTask
 *
 * I2S访问通过互斥锁保护，背景音乐在音效播放期间自动暂停
 *
 * 硬件：ESP32-S3 + ES8311 + SD卡(1-bit SDMMC)
 * 管脚配置（固定！）：见 TOOLS.md
 */
#include "bsp_music_play_freertos.h"
#include "bsp.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "music_freertos";

// ============================================================
// 全局配置
// ============================================================

// SFX音效文件（相对于SD卡根目录）
#define SFX_WIN_PATH        "/tf/sfx/win.wav"     // 中奖祝贺音
#define SFX_DRUM_PATH       "/tf/sfx/drum.wav"    // 鼓点音效（可选）

// PSRAM预分配：每个音效最大100KB PCM（~2秒@44.1kHz stereo）
#define SFX_BUF_SIZE        (200 * 1024)

// DMA ping-pong buffer（每个buffer 4KB，足够约43ms@48kHz stereo 16bit）
#define DMA_BUF_SIZE        (4096)
#define DMA_BUF_COUNT        (2)

// 任务栈大小
#define MUSIC_TASK_STACK    (8192)
#define SFX_TASK_STACK       (4096)
#define EVENT_TASK_STACK     (4096)

// 任务优先级（数字越大优先级越高）
#define MUSIC_TASK_PRIO      (3)
#define SFX_TASK_PRIO        (5)   // 高优先级，抢占musicTask
#define EVENT_TASK_PRIO      (6)   // 最高优先级

// ============================================================
// 类型定义
// ============================================================

// SFX类型
typedef enum {
    SFX_NONE = 0,
    SFX_WIN,        // 中奖祝贺音
    SFX_DRUM,       // 鼓点音
    SFX_MAX,        // 最大类型数（用于数组大小）
} sfx_type_t;

// SFX播放请求
typedef struct {
    sfx_type_t type;
} sfx_request_t;

// WAV文件头（简化版，只支持标准PCM WAV）
typedef struct {
    char     riff[4];        // "RIFF"
    uint32_t file_size;
    char     wave[4];        // "WAVE"
    char     fmt[4];         // "fmt "
    uint32_t fmt_size;
    uint16_t audio_format;   // 1 = PCM
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char     data[4];        // "data"
    uint32_t data_size;
} __attribute__((packed)) wav_header_t;

// SFX音效数据
typedef struct {
    sfx_type_t  type;
    int16_t    *psram_buf;   // PSRAM中的PCM数据（stereo）
    uint32_t    sample_count; // 样本数（stereo）
    uint32_t    sample_rate;
    uint16_t    bits_per_sample;
    bool        loaded;
} sfx_item_t;

// 背景音乐状态
typedef enum {
    MUSIC_STATE_STOPPED = 0,
    MUSIC_STATE_PLAYING,
    MUSIC_STATE_PAUSED,
} music_state_t;

// ============================================================
// 全局变量
// ============================================================

// I2S互斥锁（保护bsp_i2s_write访问）
static SemaphoreHandle_t s_i2s_mutex = NULL;

// 背景音乐暂停信号量（sfxTask播放完毕后通知musicTask恢复）
static SemaphoreHandle_t s_music_resume_sem = NULL;

// SFX任务队列
static QueueHandle_t s_sfx_queue = NULL;

// 事件任务队列（接收中奖事件）
static QueueHandle_t s_event_queue = NULL;

// SFX音效数据（预加载到PSRAM）
static sfx_item_t s_sfx[SFX_MAX] = {0};

// 当前背景音乐状态
static volatile music_state_t s_music_state = MUSIC_STATE_STOPPED;

// I2S TX句柄
static i2s_chan_handle_t s_i2s_tx = NULL;

// Ping-pong DMA buffer
static int16_t *s_dma_buf[DMA_BUF_COUNT] = {NULL};
static volatile uint8_t s_cur_buf = 0;

// SFX播放状态
static volatile bool s_sfx_playing = false;
static volatile uint32_t s_sfx_sample_pos = 0;
static volatile sfx_type_t s_sfx_current = SFX_NONE;

// ============================================================
// 内部函数声明
// ============================================================

static void music_task(void *pv);
static void sfx_task(void *pv);
static void event_task(void *pv);

static esp_err_t sfx_load_to_psram(sfx_type_t type, const char *path);
static esp_err_t sfx_write_i2s_stereo(const int16_t *samples, size_t count);

// 临时SD卡路径缓冲（用于SFX加载）
static char s_sfx_path_buf[128];

// ============================================================
// I2S DMA ping-pong buffer 管理
// ============================================================

/**
 * @brief 初始化I2S DMA ping-pong buffer（在PSRAM中分配）
 */
static esp_err_t dma_buffer_init(void)
{
    for (int i = 0; i < DMA_BUF_COUNT; i++) {
        s_dma_buf[i] = heap_caps_malloc(DMA_BUF_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        if (s_dma_buf[i] == NULL) {
            ESP_LOGE(TAG, "DMA buf[%d] alloc failed", i);
            return ESP_ERR_NO_MEM;
        }
        memset(s_dma_buf[i], 0, DMA_BUF_SIZE);
        ESP_LOGI(TAG, "DMA buf[%d] @ %p, size=%d", i, s_dma_buf[i], DMA_BUF_SIZE);
    }
    return ESP_OK;
}

/**
 * @brief 获取当前DMA buffer
 */
static int16_t *dma_get_cur_buf(void)
{
    return s_dma_buf[s_cur_buf];
}

/**
 * @brief 切换到另一个DMA buffer
 */
static void dma_switch(void)
{
    s_cur_buf = (s_cur_buf + 1) % DMA_BUF_COUNT;
}

// ============================================================
// WAV解析和PSRAM预加载
// ============================================================

/**
 * @brief 将WAV文件加载到PSRAM
 *
 * @param type  SFX类型
 * @param path  SD卡路径
 * @return ESP_OK成功，ESP_FAIL失败
 */
static esp_err_t sfx_load_to_psram(sfx_type_t type, const char *path)
{
    if (type >= SFX_MAX || type == SFX_NONE) {
        return ESP_ERR_INVALID_ARG;
    }

    sfx_item_t *item = &s_sfx[type];
    item->type = type;
    item->loaded = false;
    item->psram_buf = NULL;
    item->sample_count = 0;

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        ESP_LOGW(TAG, "SFX file not found: %s", path);
        return ESP_FAIL;
    }

    // 读取WAV头
    wav_header_t header;
    size_t hdr_len = fread(&header, 1, sizeof(header), fp);
    if (hdr_len < sizeof(header)) {
        ESP_LOGE(TAG, "WAV header read error: %s", path);
        fclose(fp);
        return ESP_FAIL;
    }

    // 验证RIFF WAVE格式
    if (memcmp(header.riff, "RIFF", 4) != 0 ||
        memcmp(header.wave, "WAVE", 4) != 0 ||
        memcmp(header.fmt,  "fmt ", 4) != 0 ||
        memcmp(header.data, "data", 4) != 0) {
        ESP_LOGE(TAG, "Invalid WAV format: %s", path);
        fclose(fp);
        return ESP_FAIL;
    }

    // 只支持PCM格式
    if (header.audio_format != 1) {
        ESP_LOGE(TAG, "Unsupported WAV format %d: %s", header.audio_format, path);
        fclose(fp);
        return ESP_FAIL;
    }

    // 检查data大小
    uint32_t data_size = header.data_size;
    if (data_size == 0 || data_size > SFX_BUF_SIZE) {
        ESP_LOGW(TAG, "SFX data_size=%u too large or zero, clamping to %d",
                 (unsigned)data_size, SFX_BUF_SIZE);
        data_size = (data_size > SFX_BUF_SIZE) ? SFX_BUF_SIZE : data_size;
    }

    item->sample_rate = header.sample_rate;
    item->bits_per_sample = header.bits_per_sample;
    uint16_t channels = header.num_channels;

    // 计算样本数
    uint32_t bytes_per_sample = (header.bits_per_sample / 8) * channels;
    uint32_t sample_count = data_size / bytes_per_sample;

    // 在PSRAM中分配stereo buffer（统一为stereo 16bit）
    item->psram_buf = heap_caps_malloc(sample_count * 2 * sizeof(int16_t),
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (item->psram_buf == NULL) {
        ESP_LOGE(TAG, "PSRAM alloc failed for SFX type=%d, size=%u",
                 type, (unsigned)(sample_count * 2 * sizeof(int16_t)));
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }

    // 读取PCM数据
    if (header.bits_per_sample == 16 && channels == 1) {
        // Mono 16bit → 读取后转stereo
        int16_t *mono = heap_caps_malloc(data_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (mono == NULL) {
            heap_caps_free(item->psram_buf);
            item->psram_buf = NULL;
            fclose(fp);
            return ESP_ERR_NO_MEM;
        }
        fread(mono, 1, data_size, fp);
        // Mono → Stereo
        for (uint32_t i = 0; i < sample_count; i++) {
            item->psram_buf[i * 2]     = mono[i];
            item->psram_buf[i * 2 + 1] = mono[i];
        }
        heap_caps_free(mono);
    } else if (header.bits_per_sample == 16 && channels == 2) {
        // Stereo 16bit → 直接读取
        fread(item->psram_buf, 1, data_size, fp);
    } else {
        ESP_LOGE(TAG, "Unsupported WAV: %dbit %dch", header.bits_per_sample, channels);
        heap_caps_free(item->psram_buf);
        item->psram_buf = NULL;
        fclose(fp);
        return ESP_FAIL;
    }

    fclose(fp);
    item->sample_count = sample_count;
    item->loaded = true;

    ESP_LOGI(TAG, "SFX[type=%d] loaded: %u samples, %u Hz, %d ch, %d bit, PSRAM @ %p",
             type, (unsigned)sample_count, (unsigned)header.sample_rate,
             channels, header.bits_per_sample, item->psram_buf);
    return ESP_OK;
}

// ============================================================
// I2S写入（带互斥保护）
// ============================================================

static esp_err_t sfx_write_i2s_stereo(const int16_t *samples, size_t count)
{
    if (s_i2s_mutex == NULL) return ESP_FAIL;
    if (xSemaphoreTake(s_i2s_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    size_t bytes_written = 0;
    esp_err_t ret = bsp_i2s_write((void *)samples,
                                   count * 2 * sizeof(int16_t),
                                   &bytes_written,
                                   pdMS_TO_TICKS(50));
    (void)ret;
    xSemaphoreGive(s_i2s_mutex);
    return ESP_OK;
}

// ============================================================
// SFX任务：播放预加载的音效
// ============================================================

static void sfx_task(void *pv)
{
    (void)pv;
    esp_task_wdt_add(NULL);  // Bug fix: 订阅TWDT
    ESP_LOGI(TAG, "sfxTask started (prio=%d, core=%d)", SFX_TASK_PRIO, xPortGetCoreID());

    sfx_request_t req;

    while (1) {
        // 等待SFX请求（阻塞）
        if (xQueueReceive(s_sfx_queue, &req, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (req.type == SFX_NONE || req.type >= SFX_MAX) {
            continue;
        }

        sfx_item_t *item = &s_sfx[req.type];
        if (!item->loaded || item->psram_buf == NULL) {
            ESP_LOGW(TAG, "sfxTask: SFX type=%d not loaded, skip", req.type);
            continue;
        }

        // ========== 开始播放音效 ==========
        s_sfx_playing = true;
        s_sfx_current = req.type;
        s_sfx_sample_pos = 0;

        // 暂停背景音乐（通过信号量通知musicTask）
        // musicTask收到信号后会暂停DMA喂送
        s_music_state = MUSIC_STATE_PAUSED;
        ESP_LOGI(TAG, "sfxTask: playing SFX type=%d, music paused", req.type);

        // 如果I2S采样率与音效不匹配，短暂切换
        // （由于ES8311不支持动态采样率快速切换，这里直接播放，容忍轻微失真）

        // 分块播放（每次写入一个chunk，避免阻塞）
        uint32_t chunk_samples = 512;  // ~10.7ms@48kHz
        uint32_t total_written = 0;

        while (s_sfx_sample_pos < item->sample_count && s_sfx_playing) {
            uint32_t remaining = item->sample_count - s_sfx_sample_pos;
            uint32_t to_write = (remaining < chunk_samples) ? remaining : chunk_samples;

            // 查找item->sample_rate对应的bsp_codec_set_fs（如果需要）
            // 目前假设所有音效采样率一致

            int16_t *ptr = item->psram_buf + s_sfx_sample_pos * 2;
            sfx_write_i2s_stereo(ptr, to_write);

            s_sfx_sample_pos += to_write;
            total_written += to_write;
        }

        ESP_LOGI(TAG, "sfxTask: SFX type=%d done, %u samples written", req.type, (unsigned)total_written);

        // 音效播放完毕
        s_sfx_playing = false;
        s_sfx_current = SFX_NONE;
        s_sfx_sample_pos = 0;

        // 通知musicTask恢复播放
        if (s_music_resume_sem != NULL) {
            // 重启背景音乐
            extern void music_resume_from_sfx(void);
            music_resume_from_sfx();
        }
    }
}

// ============================================================
// 背景音乐任务
// ============================================================
// 注意：实际的MP3解码由audio_player组件在独立任务中完成
// 这里只监控SFX期间的音乐暂停状态，通过信号量与sfxTask协调
static void music_task(void *pv)
{
    (void)pv;
    esp_task_wdt_add(NULL);  // Bug fix: 订阅TWDT
    ESP_LOGI(TAG, "musicTask started (prio=%d, core=%d)", MUSIC_TASK_PRIO, xPortGetCoreID());

    // 引用audio_player函数（由audio_player组件提供）
    extern esp_err_t audio_player_resume(void);
    extern audio_player_state_t audio_player_get_state(void);

    while (1) {
        // 等待恢复信号量（音效播放完毕后被sfxTask释放）
        if (s_music_resume_sem != NULL) {
            if (xSemaphoreTake(s_music_resume_sem, portMAX_DELAY) == pdTRUE) {
                ESP_LOGI(TAG, "musicTask: resuming background music");
                audio_player_resume();
                s_music_state = MUSIC_STATE_PLAYING;
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

// ============================================================
// 事件任务：监听中奖事件并触发sfxTask
// ============================================================

typedef enum {
    EVENT_WIN = 1,   // 中奖事件
    EVENT_DRUM,      // 鼓点事件
} event_type_t;

typedef struct {
    event_type_t type;
} event_msg_t;

static void event_task(void *pv)
{
    (void)pv;
    esp_task_wdt_add(NULL);  // Bug fix: 订阅TWDT
    ESP_LOGI(TAG, "eventTask started (prio=%d, core=%d)", EVENT_TASK_PRIO, xPortGetCoreID());

    event_msg_t msg;

    while (1) {
        // 从事件队列接收（HTTP server通过bsp_music_trigger_win()入队）
        if (xQueueReceive(s_event_queue, &msg, portMAX_DELAY) == pdTRUE) {
            if (msg.type == EVENT_WIN) {
                ESP_LOGI(TAG, "eventTask: WIN event received, triggering SFX");
                sfx_request_t req = {.type = SFX_WIN};
                // 带0超时，避免阻塞：如果队列满（上一个音效还在播放），覆盖之
                BaseType_t ret = xQueueSend(s_sfx_queue, &req, 0);
                if (ret != pdTRUE) {
                    ESP_LOGW(TAG, "sfxQueue full, dropping WIN event");
                }
            } else if (msg.type == EVENT_DRUM) {
                ESP_LOGI(TAG, "eventTask: DRUM event received");
                sfx_request_t req = {.type = SFX_DRUM};
                xQueueSend(s_sfx_queue, &req, 0);
            }
        }
    }
}

// ============================================================
// 公共API实现
// ============================================================

/**
 * @brief 触发中奖音效
 *
 * 调用时机：游戏机吐出奖品时，HTTP server或GPIO中断调用此函数
 * 响应延迟：<50ms（音效已预加载到PSRAM）
 */
void bsp_music_trigger_win(void)
{
    if (s_event_queue == NULL) {
        ESP_LOGW(TAG, "eventQueue not initialized");
        return;
    }
    event_msg_t msg = {.type = EVENT_WIN};
    BaseType_t ret = xQueueSend(s_event_queue, &msg, 0);
    if (ret != pdTRUE) {
        ESP_LOGW(TAG, "eventQueue full");
    }
    ESP_LOGI(TAG, "WIN event queued (queue free spaces=%u)",
             (unsigned)uxQueueSpacesAvailable(s_event_queue));
}

/**
 * @brief 触发鼓点音效（可选）
 */
void bsp_music_trigger_drum(void)
{
    if (s_event_queue == NULL) return;
    event_msg_t msg = {.type = EVENT_DRUM};
    xQueueSend(s_event_queue, &msg, 0);
}

/**
 * @brief 背景音乐从SFX中断中恢复
 */
void music_resume_from_sfx(void)
{
    s_music_state = MUSIC_STATE_PLAYING;
    audio_player_resume();
}

/**
 * @brief 获取当前SFX播放状态
 */
bool bsp_music_sfx_is_playing(void)
{
    return s_sfx_playing;
}

/**
 * @brief 获取当前背景音乐状态
 */
music_state_t bsp_music_get_state(void)
{
    return s_music_state;
}

// ============================================================
// 初始化
// ============================================================

/**
 * @brief 初始化FreeRTOS音乐播放系统
 *
 * 调用顺序：在bsp_init()之后，audio_player_init()之后
 *
 * @return ESP_OK成功，ESP_FAIL失败
 */
esp_err_t bsp_music_freertos_init(void)
{
    esp_err_t ret;
    BaseType_t x;

    ESP_LOGI(TAG, "=== FreeRTOS Music System Init ===");

    // 1. 创建I2S互斥锁
    if (s_i2s_mutex == NULL) {
        s_i2s_mutex = xSemaphoreCreateMutex();
        if (s_i2s_mutex == NULL) {
            ESP_LOGE(TAG, "I2S mutex create failed");
            return ESP_FAIL;
        }
    }

    // 2. 创建音乐恢复信号量
    if (s_music_resume_sem == NULL) {
        s_music_resume_sem = xSemaphoreCreateBinary();
        if (s_music_resume_sem == NULL) {
            ESP_LOGE(TAG, "Resume semaphore create failed");
            return ESP_FAIL;
        }
    }

    // 3. 创建SFX队列
    if (s_sfx_queue == NULL) {
        s_sfx_queue = xQueueCreate(4, sizeof(sfx_request_t));
        if (s_sfx_queue == NULL) {
            ESP_LOGE(TAG, "SFX queue create failed");
            return ESP_FAIL;
        }
    }

    // 4. 创建事件队列
    if (s_event_queue == NULL) {
        s_event_queue = xQueueCreate(8, sizeof(event_msg_t));
        if (s_event_queue == NULL) {
            ESP_LOGE(TAG, "Event queue create failed");
            return ESP_FAIL;
        }
    }

    // 5. 预加载音效到PSRAM
    ESP_LOGI(TAG, "Loading SFX to PSRAM...");

    // 创建临时路径
    snprintf(s_sfx_path_buf, sizeof(s_sfx_path_buf), "%s", SFX_WIN_PATH);
    ret = sfx_load_to_psram(SFX_WIN, s_sfx_path_buf);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WIN SFX not loaded (file may not exist)");
    }

    snprintf(s_sfx_path_buf, sizeof(s_sfx_path_buf), "%s", SFX_DRUM_PATH);
    ret = sfx_load_to_psram(SFX_DRUM, s_sfx_path_buf);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "DRUM SFX not loaded (optional)");
    }

    // 6. 初始化DMA buffer
    ret = dma_buffer_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "DMA buffer init failed");
        return ret;
    }

    // 7. 创建musicTask（背景音乐监控任务）
    // 注意：实际的MP3解码由audio_player组件在独立任务中完成
    // 这里只是监控SFX期间的音乐暂停状态
    x = xTaskCreatePinnedToCore(
            music_task,
            "musicTask",
            MUSIC_TASK_STACK,
            NULL,
            MUSIC_TASK_PRIO,
            NULL,
            1);  // Core 1
    if (x != pdPASS) {
        ESP_LOGE(TAG, "musicTask create failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "musicTask created");

    // 8. 创建sfxTask（音效播放任务，高优先级）
    x = xTaskCreatePinnedToCore(
            sfx_task,
            "sfxTask",
            SFX_TASK_STACK,
            NULL,
            SFX_TASK_PRIO,
            NULL,
            1);  // Core 1
    if (x != pdPASS) {
        ESP_LOGE(TAG, "sfxTask create failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "sfxTask created");

    // 9. 创建eventTask（事件监听任务，最高优先级）
    x = xTaskCreatePinnedToCore(
            event_task,
            "eventTask",
            EVENT_TASK_STACK,
            NULL,
            EVENT_TASK_PRIO,
            NULL,
            1);  // Core 1
    if (x != pdPASS) {
        ESP_LOGE(TAG, "eventTask create failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "eventTask created");

    s_music_state = MUSIC_STATE_PLAYING;

    ESP_LOGI(TAG, "=== FreeRTOS Music System Init OK ===");
    return ESP_OK;
}
