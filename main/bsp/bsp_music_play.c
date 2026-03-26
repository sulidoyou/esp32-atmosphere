#include "bsp.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "music_play";
file_iterator_instance_t *g_file_iterator = NULL;  // non-static for extern access from bsp_http_server
static audio_player_config_t player_config = {0};
static char g_current_fname[64] = "unknown";
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

    // 初始化音频播放
    player_config.mute_fn = _audio_player_mute_fn;
    player_config.write_fn = _audio_player_write_fn;
    player_config.clk_set_fn = _audio_player_std_clock;
    player_config.priority = 1;

    ESP_ERROR_CHECK(audio_player_new(player_config));
    ESP_ERROR_CHECK(audio_player_callback_register(_audio_player_callback, NULL));

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
        audio_player_play(fp);
        // 注意：不在这里fclose！audio_player内部异步读取
    } else {
        ESP_LOGE(TAG, "unable to open index %d, filename '%s'", index, filename);
    }
}

// 播放器回调
static void _audio_player_callback(audio_player_cb_ctx_t *ctx)
{
    switch (ctx->audio_event) {
    case AUDIO_PLAYER_CALLBACK_EVENT_IDLE: {
        file_iterator_next(g_file_iterator);
        int index = file_iterator_get_index(g_file_iterator);
        ESP_LOGI(TAG, "Track ended, playing index %d", index);
        play_index(index);
        break;
    }
    case AUDIO_PLAYER_CALLBACK_EVENT_PLAYING:
        pa_en(1);  // 打开功放
        break;
    case AUDIO_PLAYER_CALLBACK_EVENT_PAUSE:
        pa_en(0);  // 关闭功放
        break;
    default:
        break;
    }
}
