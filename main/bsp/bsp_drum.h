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

// 鼓模式控制来源（用于优先级管理）
typedef enum {
    DRUM_SOURCE_DEFAULT = 0,  // 默认/初始化状态，音乐模块可接管
    DRUM_SOURCE_USER,         // 用户通过HTTP/按钮设置，音乐模块不可覆盖
    DRUM_SOURCE_MUSIC,        // 音乐模块自动设置
} drum_mode_source_t;

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
    rhythm_type_t rhythm;  // 当前节奏类型
    uint8_t bpm;
    uint8_t velocity;
    uint8_t left_drum_ch;
    uint8_t right_drum_ch;
    bool running;
    uint8_t fire_limit;   // 0=无限，>0=触发N次后自动停止
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
bool bsp_drum_is_mic_sync_mode(void);
void bsp_drum_set_fire_limit(uint8_t limit);  // 0=无限，>0=触发limit次后自动停止
drum_mode_source_t bsp_drum_get_source(void);  // 获取当前模式来源

// 内部API：带来源标记的模式设置（用于music模块优先级管理）
// 若用户已锁定模式（SOURCE_USER），music模块无法接管
// 返回true表示接管成功，false表示被用户锁定
bool bsp_drum_set_mode_auto(drum_mode_t mode, drum_mode_source_t src);

// 内部API：用于Nextion/自动任务改变模式，不影响SOURCE_USER锁定状态，不自动停止鼓
// 仅当新模式和旧模式不同时才更新
void bsp_drum_set_mode_quiet(drum_mode_t mode);

// 鼓点击中回调 (drum: 0=左, 1=右)
typedef void (*drum_hit_callback_t)(uint8_t drum);
void bsp_drum_set_hit_callback(drum_hit_callback_t cb);

#endif
