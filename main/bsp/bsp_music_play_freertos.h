/**
 * @file bsp_music_play_freertos.h
 * @brief 氛围系统 FreeRTOS 多任务音乐播放 - 头文件
 *
 * 架构：
 * - musicTask  : 背景音乐播放（MP3/SD卡 + audio_player组件）
 * - sfxTask    : 音效播放（预加载WAV/PSRAM，<10ms响应）
 * - eventTask   : 中奖事件监听 → 触发 sfxTask
 *
 * 硬件：ESP32-S3 + ES8311 + SD卡(1-bit SDMMC)
 */

#ifndef __BSP_MUSIC_PLAY_FREERTOS_H__
#define __BSP_MUSIC_PLAY_FREERTOS_H__

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// 公共API
// ============================================================

/**
 * @brief 初始化FreeRTOS音乐播放系统
 *
 * 在bsp_init()之后，mp3_palyer_init()之后调用
 *
 * @return ESP_OK成功，ESP_FAIL失败
 */
esp_err_t bsp_music_freertos_init(void);

/**
 * @brief 触发中奖祝贺音效
 *
 * 调用时机：游戏机中奖时
 * 响应延迟：<50ms（音效已预加载到PSRAM，无需读SD卡）
 *
 * 线程安全：可在ISR或任何任务上下文中调用
 */
void bsp_music_trigger_win(void);

/**
 * @brief 触发鼓点音效（可选，用于鼓点同步）
 */
void bsp_music_trigger_drum(void);

/**
 * @brief 检查SFX是否正在播放
 *
 * @return true正在播放，false空闲
 */
bool bsp_music_sfx_is_playing(void);

// ============================================================
// 内部API（供musicTask/sfxTask/eventTask调用）
// ============================================================

/**
 * @brief 背景音乐从SFX中断中恢复
 *
 * sfxTask播放完毕后调用此函数
 */
void music_resume_from_sfx(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_MUSIC_PLAY_FREERTOS_H__ */
