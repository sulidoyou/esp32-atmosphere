#ifndef __BSP_WIFI_H__
#define __BSP_WIFI_H__

#include <stdbool.h>

// WiFi连接
void bsp_wifi_init(void);
bool WiFi_connected(void);
const char* WiFi_getIP(void);
void bsp_wifi_poll(void);

#endif
