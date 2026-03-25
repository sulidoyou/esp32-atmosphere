#include "bsp.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "music_play";
file_iterator_instance_t *g_file_iterator = NULL;
static char g_current_fname[64] = "unknown";
uint8_t g_sys_volume = 8;
static audio_player_config_t player_config = {0};

// GBK转UTF8简化版：ASCII透传，GBK中文→'?'占位
// 目的：避免文件名乱码，不是完整编码转换
static void gbk_to_utf8(const char *gbk, char *out, size_t out_size)
{
    size_t j = 0;
    for (size_t i = 0; gbk[i] && j + 4 < out_size - 1; i++) {
        unsigned char c = (unsigned char)gbk[i];
        if (c < 0x80) {
            out[j++] = c;  // ASCII原样透传
        } else if (c >= 0x81 && c <= 0xFE && (unsigned char)gbk[i+1] >= 0x40) {
            // GBK双字节中文 → 替换为单'?'（仅作占位）
            out[j++] = '?';
            i++;  // 跳过GBK第二字节
        } else {
            out[j++] = c;  // 保留其他字节
        }
    }
    out[j] = '\0';
}

static esp_err_t _audio_player_mute_fn(AUDIO_PLAYER_MUTE_SETTING setting)
{
    if (setting == AUDIO_PLAYER_MUTE) {
        bsp_codec_mute_set(true);
    } else {
        bsp_codec_mute_set(false);
        bsp_codec_volume_set(g_sys_volume, NULL);
    }
    return ESP_OK;
}

static esp_err_t _audio_player_write_fn(void *audio_buffer, size_t len,
                                        size_t *bytes_written, uint32_t timeout_ms)
{
    return bsp_i2s_write(audio_buffer, len, bytes_written, timeout_ms);
}

static esp_err_t _audio_player_std_clock(uint32_t rate, uint32_t bits_cfg,
                                          i2s_slot_mode_t ch)
{
    return bsp_codec_set_fs(rate, bits_cfg, ch);
}

static void play_index(int index);

// 播放器回调：每次播放事件触发
static void _audio_player_callback(audio_player_cb_ctx_t *ctx)
{
    switch (ctx->audio_event) {
    case AUDIO_PLAYER_CALLBACK_EVENT_IDLE: {
        // 一首歌播放完毕，自动切到下一首
        file_iterator_next(g_file_iterator);
        int idx = file_iterator_get_index(g_file_iterator);
        ESP_LOGI(TAG, "Track ended, auto-playing index %d", idx);
        play_index(idx);
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

// 内部：播放指定序号的音乐文件
static void play_index(int index)
{
    char filename[128];

    int retval = file_iterator_get_full_path_from_index(g_file_iterator, index,
                                                      filename, sizeof(filename));
    if (retval == 0) {
        ESP_LOGE(TAG, "get filename failed for index %d", index);
        return;
    }

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "Cannot open: %s", filename);
        return;
    }

    // 提取纯文件名（去掉路径）
    const char *slash = strrchr(filename, '/');
    const char *fname = slash ? slash + 1 : filename;

    // 提取文件名到全局缓冲区（避免播放期间filename被修改）
    gbk_to_utf8(fname, g_current_fname, sizeof(g_current_fname));
    ESP_LOGI(TAG, "Playing: %s", g_current_fname);

    // 启动播放（player会异步读取文件，调用方应立即关闭文件句柄）
    audio_player_play(fp);

    // 立即关闭文件：audio_player内部会通过fp读取数据，
    // 播放器任务持有FILE*直到文件读取完毕，不依赖原fp指针
    fclose(fp);
}

void mp3_palyer_init(void)
{
    g_file_iterator = file_iterator_new("/tf/music1");
    if (g_file_iterator == NULL) {
        ESP_LOGE(TAG, "file_iterator_new failed - check SD card");
        return;
    }

    size_t file_count = file_iterator_get_count(g_file_iterator);
    ESP_LOGI(TAG, "Found %d music files", file_count);

    player_config.mute_fn = _audio_player_mute_fn;
    player_config.write_fn = _audio_player_write_fn;
    player_config.clk_set_fn = _audio_player_std_clock;
    player_config.priority = 1;

    ESP_ERROR_CHECK(audio_player_new(player_config));
    ESP_ERROR_CHECK(audio_player_callback_register(_audio_player_callback, NULL));

    int index = file_iterator_get_index(g_file_iterator);
    ESP_LOGI(TAG, "Auto-playing index %d", index);
    play_index(index);
}

// ============ 公开API（供HTTP服务器/外部调用）============

uint8_t get_sys_volume(void)
{
    return g_sys_volume;
}

void music_play(void)
{
    if (g_file_iterator == NULL) return;
    audio_player_resume();
}

void music_pause(void)
{
    audio_player_pause();
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

void music_stop(void)
{
    audio_player_stop();
}

void music_set_volume(uint8_t vol)
{
    if (vol > 100) vol = 100;
    g_sys_volume = vol;
    bsp_codec_volume_set(vol, NULL);
}

const char *music_get_current_name(void)
{
    return g_current_fname;
}

void music_get_info(music_info_t *info)
{
    if (!info) return;
    memset(info, 0, sizeof(*info));

    if (g_file_iterator != NULL) {
        info->current_index = file_iterator_get_index(g_file_iterator);
        info->total_count = (int)file_iterator_get_count(g_file_iterator);
        strncpy(info->current_name, g_current_fname, sizeof(info->current_name) - 1);
    }

    audio_player_state_t st = audio_player_get_state();
    info->is_playing = (st == AUDIO_PLAYER_STATE_PLAYING);
}
