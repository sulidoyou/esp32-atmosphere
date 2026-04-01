#include "bsp.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "music_play";
file_iterator_instance_t *g_file_iterator = NULL;  // non-static for extern access from bsp_http_server
static audio_player_config_t player_config = {0};
static char g_current_fname[192] = "unknown";
uint8_t g_sys_volume = 8;

static esp_err_t _audio_player_mute_fn(AUDIO_PLAYER_MUTE_SETTING setting);
static esp_err_t _audio_player_write_fn(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms);
static esp_err_t _audio_player_std_clock(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch);
static void _audio_player_callback(audio_player_cb_ctx_t *ctx);
static void play_index(int index);

#define FULL_MUSIC_PATH     "/tf/music1"

void mp3_palyer_init(void)
{
    // 获取文件信息
    g_file_iterator = file_iterator_new(FULL_MUSIC_PATH);
    if(g_file_iterator == NULL)
    {
        ESP_LOGE(TAG, "file_iterator_new error - check SD card");
        return;
    }
    size_t file_count = file_iterator_get_count(g_file_iterator);
    ESP_LOGI(TAG, "find mp3:%d", file_count);
    if (file_count == 0) {
        ESP_LOGW(TAG, "no audio files found under %s", FULL_MUSIC_PATH);
        return;
    }

    // 初始化音频播放
    player_config.mute_fn = _audio_player_mute_fn;
    player_config.write_fn = _audio_player_write_fn;
    player_config.clk_set_fn = _audio_player_std_clock;
    player_config.priority = 1;

    esp_err_t ret = audio_player_new(player_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "audio_player_new failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = audio_player_callback_register(_audio_player_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "audio_player_callback_register failed: %s", esp_err_to_name(ret));
        return;
    }

    int index = file_iterator_get_index(g_file_iterator);
    ESP_LOGI(TAG, "playing index '%d'", index);
    play_index(index);
}

// 返回当前音量
uint8_t get_sys_volume(void)
{
    return g_sys_volume;
}

// 设置音量
void music_set_volume(uint8_t vol)
{
    if (vol > 100) vol = 100;
    g_sys_volume = vol;
    bsp_codec_volume_set(vol, NULL);
}

// 获取当前曲目名
const char *music_get_current_name(void)
{
    return g_current_fname;
}

// 公开控制API
void music_play(void)
{
    if (g_file_iterator == NULL) return;
    audio_player_resume();
}

void music_pause(void)
{
    audio_player_pause();
}

void music_stop(void)
{
    audio_player_stop();
}

void music_next(void)
{
    if (g_file_iterator == NULL) return;
    file_iterator_next(g_file_iterator);
    play_index(file_iterator_get_index(g_file_iterator));
}

void music_prev(void)
{
    if (g_file_iterator == NULL) return;
    file_iterator_prev(g_file_iterator);
    play_index(file_iterator_get_index(g_file_iterator));
}

void music_play_index(int idx)
{
    if (g_file_iterator == NULL) return;
    size_t total = file_iterator_get_count(g_file_iterator);
    if (idx < 0) idx = 0;
    if (idx >= (int)total) idx = total - 1;
    file_iterator_set_index(g_file_iterator, idx);
    play_index(idx);
}

void music_reset(void)
{
    if (g_file_iterator == NULL) return;
    int idx = file_iterator_get_index(g_file_iterator);
    audio_player_stop();
    play_index(idx);
}

// ==================== 内部实现 ====================

// 声音处理函数
static esp_err_t _audio_player_mute_fn(AUDIO_PLAYER_MUTE_SETTING setting)
{
    bsp_codec_mute_set(setting == AUDIO_PLAYER_MUTE ? true : false);
    if (setting == AUDIO_PLAYER_UNMUTE) {
        bsp_codec_volume_set(g_sys_volume, NULL);
    }
    return ESP_OK;
}

// 音频数据写入I2S
static esp_err_t _audio_player_write_fn(void *audio_buffer, size_t len,
                                        size_t *bytes_written, uint32_t timeout_ms)
{
    return bsp_i2s_write(audio_buffer, len, bytes_written, timeout_ms);
}

// 采样率设置
static esp_err_t _audio_player_std_clock(uint32_t rate, uint32_t bits_cfg,
                                          i2s_slot_mode_t ch)
{
    return bsp_codec_set_fs(rate, bits_cfg, ch);
}

// 播放指定序号的音乐
static void play_index(int index)
{
    ESP_LOGI(TAG, "play_index(%d)", index);
    if (g_file_iterator == NULL) {
        ESP_LOGE(TAG, "play_index: g_file_iterator is NULL");
        return;
    }

    char filename[128];
    int retval = file_iterator_get_full_path_from_index(g_file_iterator, index,
                                                        filename, sizeof(filename));
    if (retval == 0) {
        ESP_LOGE(TAG, "unable to retrieve filename");
        return;
    }

    FILE *fp = fopen(filename, "rb");
    if (fp) {
        // 提取纯文件名
        const char *slash = strrchr(filename, '/');
        const char *fname = slash ? slash + 1 : filename;
        strncpy(g_current_fname, fname, sizeof(g_current_fname) - 1);
        g_current_fname[sizeof(g_current_fname) - 1] = '\0';
        ESP_LOGI(TAG, "Playing '%s'", g_current_fname);
        // audio_player_play后，audio_player会异步读取数据
        // 读取完成后audio_player内部会关闭fp，这里不需要也不应该fclose
        // 否则会导致audio_player读取时fd已关闭
        esp_err_t ret = audio_player_play(fp);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "audio_player_play failed: %s", esp_err_to_name(ret));
            fclose(fp);
            return;
        }
    } else {
        ESP_LOGE(TAG, "unable to open index %d, filename '%s'", index, filename);
    }
}

// 播放器回调
static void _audio_player_callback(audio_player_cb_ctx_t *ctx)
{
    switch (ctx->audio_event) {
    case AUDIO_PLAYER_CALLBACK_EVENT_IDLE: {
        if (g_file_iterator == NULL) {
            break;
        }
        file_iterator_next(g_file_iterator);
        int index = file_iterator_get_index(g_file_iterator);
        ESP_LOGI(TAG, "Track ended, playing index %d", index);
        play_index(index);
        break;
    }
    case AUDIO_PLAYER_CALLBACK_EVENT_PLAYING: {
        pa_en(1);  // 打开功放
        // 音乐播放 → 自动启动鼓节奏（音乐同步模式）
        // 若用户已锁定PRESET/MANUAL模式（SOURCE_USER），音乐无法接管
        if (bsp_drum_set_mode_auto(DRUM_MODE_MUSIC_SYNC, DRUM_SOURCE_MUSIC)) {
            bsp_drum_start();
        }
        break;
    }
    case AUDIO_PLAYER_CALLBACK_EVENT_PAUSE: {
        pa_en(0);  // 关闭功放
        // 音乐暂停 → 仅停止音乐自己启动的鼓节奏
        // 若鼓被用户锁定（SOURCE_USER），不干预
        if (bsp_drum_get_source() == DRUM_SOURCE_MUSIC) {
            bsp_drum_stop();
        }
        break;
    }
    case AUDIO_PLAYER_CALLBACK_EVENT_COMPLETED_PLAYING_NEXT:
    case AUDIO_PLAYER_CALLBACK_EVENT_SHUTDOWN:
    case AUDIO_PLAYER_CALLBACK_EVENT_UNKNOWN_FILE_TYPE:
    case AUDIO_PLAYER_CALLBACK_EVENT_UNKNOWN:
    default:
        break;
    }
}
