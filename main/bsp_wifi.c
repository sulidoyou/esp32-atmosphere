#include "bsp_wifi.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

static const char *TAG = "WiFi";
static bool s_connected = false;
static char s_ip_str[20] = {0};

// 设备静态IP配置（需要和路由器在同一网段）
#define DEVICE_IP      "192.168.0.247"
#define DEVICE_NETMASK "255.255.255.0"
#define DEVICE_GW     "192.168.0.1"

static esp_netif_t *s_sta_netif = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        ESP_LOGI(TAG, "WiFi disconnected");
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        sprintf(s_ip_str, IPSTR, IP2STR(&event->ip_info.ip));
        s_connected = true;
        ESP_LOGI(TAG, "WiFi connected! IP: %s", s_ip_str);
    }
}

void bsp_wifi_init(void)
{
    ESP_LOGI(TAG, "WiFi init...");

    // 烧录后软件复位需要等待WiFi PHY稳定
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 清理可能残留的WiFi状态
    esp_wifi_disconnect();
    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(200));

    // 1. NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) { ESP_LOGW(TAG, "NVS fail"); return; }
    ESP_LOGI(TAG, "NVS OK");

    // 2. 初始化 netif 子系统
    ret = esp_netif_init();
    if (ret != ESP_OK) { ESP_LOGW(TAG, "esp_netif_init fail"); return; }
    ESP_LOGI(TAG, "esp_netif_init OK");

    // 3. 创建默认事件循环
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "esp_event_loop_create_default %d", ret);
    } else {
        ESP_LOGI(TAG, "event_loop OK");
    }

    // 4. 创建 WiFi Station netif
    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (s_sta_netif == NULL) {
        ESP_LOGW(TAG, "netif NULL");
    } else {
        ESP_LOGI(TAG, "netif created OK");
        // 停止DHCP客户端
        ret = esp_netif_dhcpc_stop(s_sta_netif);
        ESP_LOGI(TAG, "dhcpc_stop: %d", ret);
        // 设置静态IP
        esp_netif_ip_info_t ip_info;
        memset(&ip_info, 0, sizeof(ip_info));
        ip_info.ip.addr = ipaddr_addr(DEVICE_IP);
        ip_info.netmask.addr = ipaddr_addr(DEVICE_NETMASK);
        ip_info.gw.addr = ipaddr_addr(DEVICE_GW);
        ret = esp_netif_set_ip_info(s_sta_netif, &ip_info);
        ESP_LOGI(TAG, "set static ip %s: %d", DEVICE_IP, ret);
    }

    // 5. WiFi驱动
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) { ESP_LOGW(TAG, "wifi init fail"); return; }
    ESP_LOGI(TAG, "wifi_init OK");

    // 5.1 禁用WiFi功耗管理（直供电，不需要省电，降低延迟）
    ret = esp_wifi_set_ps(WIFI_PS_NONE);
    ESP_LOGI(TAG, "WiFi PS disabled: %d", ret);

    // 6. 注册事件处理器
    ret = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START,
                                    &wifi_event_handler, NULL);
    ret = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                    &wifi_event_handler, NULL);
    ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                    &ip_event_handler, NULL);

    // 7. 配置WiFi
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    memcpy(wifi_config.sta.ssid, "freeluck6", 9);
    memcpy(wifi_config.sta.password, "freeluck520", 12);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    ret |= esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    ret |= esp_wifi_start();
    ESP_LOGI(TAG, "start: %d", ret);

    strcpy(s_ip_str, DEVICE_IP);
    ESP_LOGI(TAG, "WiFi static IP: %s", DEVICE_IP);
}

void bsp_wifi_poll(void)
{
    // WiFi状态轮询（无打印，打印由main.c统一处理：只在状态变化时）
}

bool WiFi_connected(void)
{
    return s_connected;
}

const char* WiFi_getIP(void)
{
    return s_ip_str;
}
