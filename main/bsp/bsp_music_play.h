#ifndef _BSP_MUSIC_PLAY_H_
#define _BSP_MUSIC_PLAY_H_

#include <stdint.h>
#include <stdbool.h>
#include "file_iterator.h"

extern uint8_t g_sys_volume;
extern file_iterator_instance_t *g_file_iterator;

// 音乐信息结构体
typedef struct {
    int current_index;       // 当前曲目序号（0起始）
    int total_count;         // 总曲目数
    char current_name[64];   // 当前曲目名（UTF-8，可能含?占位符）
    bool is_playing;          // 是否正在播放
} music_info_t;

void mp3_palyer_init(void);
uint8_t get_sys_volume(void);

// 音乐控制API
void music_play(void);
void music_pause(void);
void music_stop(void);
void music_next(void);
void music_prev(void);
void music_play_index(int idx);  // 直接播放指定序号（0起始）
void music_reset(void);           // 重置当前歌曲：停止并从头开始播放
void music_set_volume(uint8_t vol);
const char *music_get_current_name(void);
void music_get_info(music_info_t *info);  // 获取完整播放信息

#endif
