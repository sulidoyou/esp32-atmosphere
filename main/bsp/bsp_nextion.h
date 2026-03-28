/*
 * BSP - Nextion串口屏驱动
 * GPIO43=TX, GPIO44=RX
 * 波特率: 115200
 */

#ifndef __BSP_NEXTION_H__
#define __BSP_NEXTION_H__

#include <stdint.h>
#include <stdbool.h>

// Nextion屏幕初始化
void bsp_nextion_init(void);

// 更新系统状态显示
// mode: 0=idle, 1=breathing, 2=rainbow, 3=preset, 4=manual, 5=mic_sync
void bsp_nextion_update_system(bool running, uint8_t mode);

// 更新网络状态
// connected: WiFi是否连接
// ip_str: IP地址字符串
void bsp_nextion_update_network(bool connected, const char *ip_str);

// 更新场景信息
// scene_name: 场景名称 (最多10字符)
// scene_id: 场景编号
void bsp_nextion_update_scene(const char *scene_name, uint8_t scene_id);

// 鼓点节拍可视化
// beat: 当前鼓点 0=左, 1=右, 2=停止
// bpm: 当前BPM
void bsp_nextion_update_beat(uint8_t beat, uint8_t bpm);

// 更新BPM显示
void bsp_nextion_update_bpm(uint8_t bpm);

// 鼓点可视化回调 (注册后鼓点击中时自动触发)
void bsp_nextion_set_drum_callback(void (*cb)(uint8_t drum));

// 刷新所有显示
void bsp_nextion_refresh(void);

// 发送Nextion指令 (内部使用)
void bsp_nextion_send_cmd(const char *cmd);

#endif
