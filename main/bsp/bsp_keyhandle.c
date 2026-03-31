

#include "bsp.h"

static const char *TAG = "KEY";



void KeyHandle(void)
{
    uint8_t keyValue = 0;
    keyValue = bsp_GetKey();  // 获取按键值
    if(keyValue == 0)
    {
        return;
    }
    switch (keyValue)
    {
    case KEY_1_DOWN:
        g_sys_volume = (g_sys_volume+10)%50;
        ESP_LOGI(TAG, "volume: %d", g_sys_volume);
        bsp_codec_volume_set(g_sys_volume, NULL);
        break;
     case KEY_2_DOWN:
        
        break;
    case KEY_3_DOWN:
        
        break;   
    default:
        break;
    }
}













