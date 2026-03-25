
#ifndef __BSP_ES8311_H
#define __BSP_ES8311_H

void bsp_es8311_init(void);

/**
 * @brief  写入音频数据的函数原型
 *
 * @param file_path 写入音频文件路径
 * @param datas 写入数据
 * @param size 写入长度
 * @param max 最大写入长度
 * @return 返回值为实际写入 ， 0：结束
 *
 */
typedef uint16_t (*AUDIO_FILE_WRITE)(const char *file_path, uint8_t *datas, uint16_t size, size_t max);
/**
 * @brief 录音
 *
 * @param file_path 写入音频文件路径
 * @param write_cb  写入音频数据的函数原型
 * @param gain    录音增益 0-42
 * @param size    最大录音长度
 *
 * @return esp_err_t ESP_OK 成功
 */
esp_err_t bsp_codec_record(AUDIO_FILE_WRITE write_cb, const char *file_path, float gain, size_t max);
/**
 * @brief 读取音频数据的函数原型
 *
 * @param file_path 读取音频文件路径
 * @param datas 读取数据
 * @param size 读取长度
 * @return  返回值为实际读取值，0：结束
 *
 */
typedef uint16_t (*AUDIO_FILE_READ)(const char *file_path, uint8_t *datas, uint16_t size);

/**
 * @brief 播放音频
 *
 * @param file_path 读取音频文件路径
 * @param read_cb 读取音频数据的函数原型
 * @param vol 音频播放声音 0-100
 *
 * @return esp_err_t ESP_OK 成功
 */
esp_err_t bsp_codec_play(AUDIO_FILE_READ read_cb, const char *file_path, uint8_t vol);

/**
 * @brief 音频调节
 *
 * @param vol 音频播放声音 0-100
 * @return bool True 成功
 */
bool bsp_codec_set_vol(uint8_t vol);


esp_err_t bsp_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch);
// 播放音乐
esp_err_t bsp_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms);
// 设置静音与否
esp_err_t bsp_codec_mute_set(bool enable);
// 设置喇叭音量
esp_err_t bsp_codec_volume_set(int volume, int *volume_set);



#endif





















