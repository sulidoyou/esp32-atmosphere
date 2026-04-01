
#include "bsp.h"
static const char *TAG = "BSP_AUDIO";
#define GPIO_I2S_MCK       (GPIO_NUM_38)
#define GPIO_I2S_BCK       (GPIO_NUM_14)
#define GPIO_I2S_WS        (GPIO_NUM_13)
#define GPIO_I2S_DO        (GPIO_NUM_45)
#define GPIO_I2S_DI        (GPIO_NUM_12)
i2s_chan_handle_t i2stx_handle = NULL;
i2s_chan_handle_t i2srx_handle = NULL;
void bsp_i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    esp_err_t ret =i2s_new_channel(&chan_cfg, &i2stx_handle, &i2srx_handle);
     if (ret != ESP_OK || i2stx_handle == NULL) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(ret));
        return;
    }   
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(48000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(16, I2S_SLOT_MODE_STEREO),
        .gpio_cfg ={
            .mclk = GPIO_I2S_MCK,
            .bclk = GPIO_I2S_BCK,
            .ws = GPIO_I2S_WS,
            .dout = GPIO_I2S_DO,
            .din = GPIO_I2S_DI,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };   
    ret = i2s_channel_init_std_mode(i2stx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_txchannel_init_std_mode failed: %s", esp_err_to_name(ret));
        return;
    }
     ret = i2s_channel_init_std_mode(i2srx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_rxchannel_init_std_mode failed: %s", esp_err_to_name(ret));
        return;
    }   
    ret = i2s_channel_enable(i2stx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_txchannel_enable failed: %s", esp_err_to_name(ret));
        return;
    }
     ret = i2s_channel_enable(i2srx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_rxchannel_enable failed: %s", esp_err_to_name(ret));
        return;
    }   
    ESP_LOGI(TAG, "I2S initialized successfully");
}

// 播放PCM采样（非阻塞，直接写I2S TX）
void bsp_i2s_play_pcm(const int16_t *samples, int count)
{
    if (!i2stx_handle || samples == NULL || count <= 0) return;
    size_t bytes_written = 0;
    // 输入为16bit mono，分块复制为stereo (L=R)，避免VLA栈溢出
    const int chunk_samples = 256;
    int16_t stereo[chunk_samples * 2];

    for (int offset = 0; offset < count; offset += chunk_samples) {
        int this_chunk = count - offset;
        if (this_chunk > chunk_samples) {
            this_chunk = chunk_samples;
        }

        for (int i = 0; i < this_chunk; i++) {
            int16_t v = samples[offset + i];
            stereo[i * 2] = v;
            stereo[i * 2 + 1] = v;
        }

        i2s_channel_write(i2stx_handle,
                          stereo,
                          this_chunk * 2 * sizeof(int16_t),
                          &bytes_written,
                          pdMS_TO_TICKS(50));
    }
}








