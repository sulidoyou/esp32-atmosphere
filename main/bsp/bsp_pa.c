// PA功放控制stub (硬件无PCA9557，简化处理)
// PA默认开启，不需要控制

#include "esp_log.h"

void pa_en(uint8_t level)
{
    (void)level;  // 避免警告
}
