#include "bsp.h"
#define TAG "CODEC_8311"
esp_codec_dev_handle_t _codec_dev;

#define CODEC_DEFAULT_SAMPLE_RATE          (48000)
#define CODEC_DEFAULT_BIT_WIDTH            (16)
#define CODEC_DEFAULT_ADC_VOLUME           (24.0)
#define CODEC_DEFAULT_CHANNEL              (2)
// 设置采样率
esp_err_t bsp_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    esp_err_t ret = ESP_OK;

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = rate,
        .channel = ch,
        .bits_per_sample = bits_cfg,
    };
    
    if (_codec_dev) {
        ret = esp_codec_dev_close(_codec_dev);
    }
    // if (record_dev_handle) {
    //     ret |= esp_codec_dev_close(record_dev_handle);
    //     ret |= esp_codec_dev_set_in_gain(record_dev_handle, CODEC_DEFAULT_ADC_VOLUME);
    // }

    if (_codec_dev) {
        ret |= esp_codec_dev_open(_codec_dev, &fs);
    }
    // if (record_dev_handle) {
    //     ret |= esp_codec_dev_open(record_dev_handle, &fs);
    // }
    return ret;
}
void bsp_es8311_init(void)
{
    // 1.绑定codec组件的控制通道 ， 控制命令
    audio_codec_i2c_cfg_t codec_i2c_cfg = {
        .port = I2C_NUM_1,          // 通信的总线
        .addr = 0x30,         // ES8311的地址
        .bus_handle = i2c_bus_handle // 通信的总线权柄
    };    

    // 把ES8311设备添加到I2C总线上
    const audio_codec_ctrl_if_t *_ctrl_if = audio_codec_new_i2c_ctrl(&codec_i2c_cfg);
    if (_ctrl_if == NULL)
    {
        ESP_LOGW(TAG, "out_ctrl_if = audio_codec_new_i2c_ctrl  is NULL");
        return;
    }
    // 2. 创建ES8311编解码器， 用于操作 ES8311 寄存器
    audio_codec_gpio_if_t *_gpio_if = audio_codec_new_gpio();

    es8311_codec_cfg_t es8311_cfg = {
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH, // 既要输入，又要输出
        .ctrl_if = _ctrl_if,                        // 控制接口 -  I2C
        .gpio_if = _gpio_if,                        // GPIO控制接口 @pa_pin
        .pa_pin = GPIO_NUM_NC,                     // 控制 NS4150 开关
        .pa_reverted = false,                       // 控制 NS4150 开关电平   高电平有效 ，不反转
        .use_mclk = true,                           // 主机模式，默认主时钟
        .mclk_div = 0,                              // 默认分频 256
    };
    const audio_codec_if_t *_es8311_codec_if = es8311_codec_new(&es8311_cfg);
    if (_es8311_codec_if == NULL)
    {
        ESP_LOGW(TAG, "out_codec_if = es8311_codec_new  is NULL");
        return ;
    }
    ESP_LOGI(TAG, "audio_codec_if_t init success");
    // 3. 绑定codec组件的数据通道
    audio_codec_i2s_cfg_t codec_i2s_cfg = {
        .port = I2S_NUM_0,  // 通信口
        .tx_handle = i2stx_handle, // 发送通道
        .rx_handle = i2srx_handle  // 接收通道
    };
    const audio_codec_data_if_t *_data_if = audio_codec_new_i2s_data(&codec_i2s_cfg);
    if (_data_if == NULL)
    {
        ESP_LOGW(TAG, "data_if = audio_codec_new_i2s_data  is NULL");
        return ;
    }
    ESP_LOGI(TAG, "audio_codec_data_if_t init success");
    // 4.初始化 ESP 编解码库 , 统一接口调用
    esp_codec_dev_cfg_t dev_cfg = {
        .codec_if = _es8311_codec_if,         // es8311_codec_new 获取到的接口实现
        .data_if = _data_if,                  // audio_codec_new_i2s_data 获取到的数据接口实现
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT // 设备同时支持录制和播放
    };
    _codec_dev = esp_codec_dev_new(&dev_cfg); // 生成编解码器设备 -> 最终的控制权柄
    if (_codec_dev == NULL)
    {
        ESP_LOGW(TAG, "_codec_dev = esp_codec_dev_new is NULL");
        return ;
    }
    ESP_LOGI(TAG, "esp_codec_dev_handle_t init success");
     bsp_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE, CODEC_DEFAULT_BIT_WIDTH, CODEC_DEFAULT_CHANNEL);
}

#define ESP_RETURN_ON_ERROR(x, log_tag, format, ...) do { \
    esp_err_t err_rc_ = (x); \
    if (unlikely(err_rc_ != ESP_OK)) { \
        ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        return err_rc_; \
    } \
} while(0)
#define BUFFER_SIZE (240 * 2)
/**
 * @brief 音频格式信息
 *
 */
static esp_codec_dev_sample_info_t fs = {
    .sample_rate = 48000,//44100,                // 采样率（如8000, 16000, 44100等）,每秒数据量：44,100 × 2 × 2 = 184,400 字节/秒
    .channel = 2, // 声道（单声道/立体声） ES8311不支持双声道
    .bits_per_sample = 16,               // 位深（16/24位）
    .mclk_multiple = 0                   // If value is 0, mclk = sample_rate * 256
};

void bsp_codec_set_simple_info(uint16_t sample_rate , uint8_t bits_per_sample)
{
	fs.sample_rate = sample_rate;
	fs.bits_per_sample = bits_per_sample;
}

bool bsp_codec_set_vol(uint8_t vol)
{
    ESP_LOGD(TAG, "set speaker vol %d", vol);
    return (esp_codec_dev_set_out_vol(_codec_dev, vol) == ESP_CODEC_DEV_OK);
}

esp_err_t bsp_codec_play(AUDIO_FILE_READ read_cb, const char *file_path, uint8_t vol)
{
    esp_err_t res = ESP_OK;
    if (read_cb == NULL)
    {
        ESP_LOGW(TAG, "read_cb is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    // 打开播放通道
    ESP_RETURN_ON_ERROR(esp_codec_dev_open(_codec_dev, &fs), TAG, "esp_codec_dev_open failure");
    // 设置音量
    ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_vol(_codec_dev, vol), TAG, "esp_codec_dev_set_out_vol failure");
    // 推送播放数据
    uint8_t data[BUFFER_SIZE];
    uint16_t rlen;
    do
    {
        rlen = read_cb(file_path, data, BUFFER_SIZE); // 打开文件、关闭文件、读取文件、读取数据
        if (rlen > 0)
        {
            res = esp_codec_dev_write(_codec_dev, data, rlen); // 推送数据
            if (ESP_CODEC_DEV_OK != res)
            {
                ESP_LOGW(TAG, "esp_codec_dev_write failure : %#x", res);
                break;
            }
        }
    } while (rlen);
    // 关闭通道
    res = esp_codec_dev_close(_codec_dev);
    return res;
}



esp_err_t bsp_codec_record(AUDIO_FILE_WRITE write_cb, const char *file_path, float gain, size_t max)
{
    uint8_t data[BUFFER_SIZE];
    esp_err_t res = ESP_CODEC_DEV_OK;
    if (write_cb == NULL)
    {
        ESP_LOGW(TAG, "write_cb is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGW(TAG, "start record");
    // 打开录制通道
    ESP_RETURN_ON_ERROR(esp_codec_dev_open(_codec_dev, &fs), TAG, "esp_codec_dev_open failure");
    ESP_RETURN_ON_ERROR(esp_codec_dev_set_in_gain(_codec_dev, gain), TAG, "esp_codec_dev_set_in_gain failure");
    while (1)
    {
        res = esp_codec_dev_read(_codec_dev, data, BUFFER_SIZE);//读取麦克风数据
        if (ESP_CODEC_DEV_OK == res)
        {
            if (write_cb(file_path, data, BUFFER_SIZE, max) != BUFFER_SIZE) // 写入文件结束
                break;
        }
        else // 读取音频出错
        {
            write_cb(file_path, data, 0, 0);//文件权柄释放
            ESP_LOGW(TAG, "esp_codec_dev_write failure : %#x", res);
            break;
        }
    }
    ESP_LOGW(TAG, "end record");
    //关闭录制通道
    esp_codec_dev_close(_codec_dev);
    return res;
}






// 播放音乐
esp_err_t bsp_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    ret = esp_codec_dev_write(_codec_dev, audio_buffer, len);
    *bytes_written = len;
    return ret;
}

// 设置静音与否
esp_err_t bsp_codec_mute_set(bool enable)
{
    esp_err_t ret = ESP_OK;
    ret = esp_codec_dev_set_out_mute(_codec_dev, enable);
    return ret;
}

// 设置喇叭音量
esp_err_t bsp_codec_volume_set(int volume, int *volume_set)
{
    esp_err_t ret = ESP_OK;
    float v = volume;
    ret = esp_codec_dev_set_out_vol(_codec_dev, (int)v);
    return ret;
}







