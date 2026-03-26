#ifndef __BSP_MIC_H__
#define __BSP_MIC_H__

// 麦克风拍手检测模块
// 使用I2S RX (GPIO12 INMP441) 读取麦克风数据
// 检测到音量突发（拍手）时触发回调

/**
 * @brief 设置拍手触发回调
 * @param cb 回调函数，检测到拍手时调用
 */
void bsp_mic_set_clap_callback(void (*cb)(void));

/**
 * @brief 启用/禁用麦克风拍手检测
 * @param en true=启用，false=禁用
 */
void bsp_mic_enable(bool en);

/**
 * @brief 麦克风检测任务（内部使用）
 */
void bsp_mic_task(void *pv);

#endif  // __BSP_MIC_H__
