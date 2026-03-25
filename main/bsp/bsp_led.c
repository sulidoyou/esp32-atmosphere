#include "bsp.h"


#define LED1_GPIO_NUM        GPIO_NUM_3    
#define LED2_GPIO_NUM        GPIO_NUM_46   

/**
 * @brief LED 闪烁任务函数
 * @param pvParameters 任务参数（此处未使用）
 */
void led_blink_task(void *pvParameters)
{


  
    while (1) {
        // 点亮 LED（GPIO 输出高电平，若 LED 是低电平点亮则改为 0）
        gpio_set_level(LED1_GPIO_NUM, 1);
        gpio_set_level(LED2_GPIO_NUM, 1);
        vTaskDelay(pdMS_TO_TICKS(500));  // 延时 500ms

        // 熄灭 LED
        gpio_set_level(LED1_GPIO_NUM, 0);
        gpio_set_level(LED2_GPIO_NUM, 0);
        vTaskDelay(pdMS_TO_TICKS(500));  // 延时 500ms
    }

    // 任务退出（实际不会执行到这里）
    vTaskDelete(NULL);
}
void bsp_led_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED1_GPIO_NUM)|(1ULL << LED2_GPIO_NUM),  // 选中目标引脚
        .mode = GPIO_MODE_OUTPUT,                // 输出模式
        .pull_up_en = GPIO_PULLUP_DISABLE,       // 禁用上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE,   // 禁用下拉
        .intr_type = GPIO_INTR_DISABLE           // 禁用中断
    };
    gpio_config(&io_conf);

    xTaskCreate(
        led_blink_task,    // 任务函数
        "led_blink",       // 任务名称（仅调试用）
        2048,              // 任务栈大小（5.3 中 2048 足够）
        NULL,              // 任务参数
        5,                 // 任务优先级（5 为中等）
        NULL               // 任务句柄（无需保存则传 NULL）
    );    
}


























