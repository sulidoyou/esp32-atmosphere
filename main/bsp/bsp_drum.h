#ifndef __BSP_DRUM_H__
#define __BSP_DRUM_H__

#include <stdint.h>
#include <stdbool.h>

// 鼓模式
typedef enum {
    DRUM_MODE_NONE = 0,
    DRUM_MODE_PRESET,    // 预设节奏
    DRUM_MODE_MANUAL,     // 手动
    DRUM_MODE_MIC_SYNC,   // 麦克风同步
    DRUM_MODE_MUSIC_SYNC, // 音乐同步(SD卡)
} drum_mode_t;

// 预设节奏类型
typedef enum {
    RHYTHM_SINGLE = 0,   // 单击
    RHYTHM_DOUBLE,       // 双击
    RHYTHM_ROLL,         // 滚奏
    RHYTHM_WALTZ,        // 华尔兹
    RHYTHM_ROCK,         // 摇滚
} rhythm_type_t;

// 鼓信息结构
typedef struct {
    drum_mode_t mode;
    uint8_t bpm;
    uint8_t velocity;
    uint8_t left_drum_ch;
    uint8_t right_drum_ch;
    bool running;
} drum_info_t;

void bsp_drum_init(void);
void bsp_drum_get_info(drum_info_t *info);
void bsp_drum_set_mode(drum_mode_t mode);
void bsp_drum_set_bpm(uint8_t bpm);
void bsp_drum_set_velocity(uint8_t vel);
void bsp_drum_set_rhythm(rhythm_type_t type);
void bsp_drum_start(void);
void bsp_drum_stop(void);
void bsp_drum_hit(uint8_t drum);

#endif
