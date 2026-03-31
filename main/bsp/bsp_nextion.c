/*
 * BSP - Nextion串口屏驱动
 * GPIO43=TX, GPIO44=RX (ESP32-S3有效GPIO)
 * 波特率: 115200
 * 
 * Nextion协议:
 * - 文本: "page0.t0.txt=\"hello\""
 * - 数值: "page0.n0.val=123"
 * - 结束符: 3字节 0xFF 0xFF 0xFF
 */

#include "bsp_nextion.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "NEXTION";

// Nextion UART配置
#define NEXTION_UART_NUM      UART_NUM_2
#define NEXTION_TX_GPIO       GPIO_NUM_43
#define NEXTION_RX_GPIO       GPIO_NUM_44
#define NEXTION_BAUD_RATE     115200
#define NEXTION_BUF_SIZE      1024

// Nextion结束符 (3字节 0xFF)
static const uint8_t NEXTION_END[3] = {0xFF, 0xFF, 0xFF};

// 任务队列
static QueueHandle_t s_nextion_queue = NULL;

// 显示状态缓存 (减少刷屏)
typedef struct {
    bool running;
    uint8_t mode;
    bool network_ok;
    char ip_str[16];
    char scene_name[16];
    uint8_t scene_id;
    uint8_t beat;
    uint8_t bpm;
} nextion_state_t;

static nextion_state_t s_state = {
    .running = false,
    .mode = 0,
    .network_ok = false,
    .ip_str = "0.0.0.0",
    .scene_name = "Idle",
    .scene_id = 0,
    .beat = 2,
    .bpm = 120,
};

// 显示模式名称
static const char* MODE_NAMES[] = {
    "Idle", "Breath", "Rainbow", "Preset", "Manual", "MicSync"
};
#define MODE_NAMES_COUNT (sizeof(MODE_NAMES)/sizeof(MODE_NAMES[0]))

// 鼓点可视化命令 (内部)
static void _update_text_component(const char *comp, const char *value)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "%s.txt=\"%s\"", comp, value);
    bsp_nextion_send_cmd(cmd);
}

static void _update_num_component(const char *comp, int value)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "%s.val=%d", comp, value);
    bsp_nextion_send_cmd(cmd);
}

// 鼓点回调 (被鼓点任务调用)
static void __attribute__((unused)) _on_drum_hit(uint8_t drum)
{
    // beat: 0=左, 1=右, 2=停止
    uint8_t beat_val = (drum == 0) ? 0 : (drum == 1) ? 1 : 2;
    _update_num_component("p0.n2", beat_val);
    _update_num_component("p0.n3", drum == 0 ? 1 : 0);  // 左LED
    _update_num_component("p0.n4", drum == 1 ? 1 : 0);  // 右LED
    
    ESP_LOGD(TAG, "Drum hit visual: drum=%d", drum);
}

// 鼓点回调注册 (供外部调用)
static void (*s_drum_callback)(uint8_t drum) = NULL;

void bsp_nextion_set_drum_callback(void (*cb)(uint8_t drum))
{
    s_drum_callback = cb;
}

// 发送Nextion指令
void bsp_nextion_send_cmd(const char *cmd)
{
    uart_write_bytes(NEXTION_UART_NUM, cmd, strlen(cmd));
    uart_write_bytes(NEXTION_UART_NUM, NEXTION_END, 3);
}

// 更新系统状态
void bsp_nextion_update_system(bool running, uint8_t mode)
{
    if (s_state.running != running || s_state.mode != mode) {
        s_state.running = running;
        s_state.mode = mode;
        
        const char *status = running ? "Running" : "Stopped";
        _update_text_component("p0.t1", status);
        _update_text_component("p0.t2", MODE_NAMES[mode < MODE_NAMES_COUNT ? mode : 0]);
        
        ESP_LOGD(TAG, "System: %s, Mode: %s", status, MODE_NAMES[mode < MODE_NAMES_COUNT ? mode : 0]);
    }
}

// 更新网络状态
void bsp_nextion_update_network(bool connected, const char *ip_str)
{
    if (s_state.network_ok != connected || strcmp(s_state.ip_str, ip_str) != 0) {
        s_state.network_ok = connected;
        strncpy(s_state.ip_str, ip_str, sizeof(s_state.ip_str) - 1);
        s_state.ip_str[sizeof(s_state.ip_str) - 1] = 0;
        
        const char *net_status = connected ? ip_str : "Offline";
        _update_text_component("p0.t3", net_status);
        
        // WiFi LED图标: 0=断开, 1=连接
        _update_num_component("p0.n0", connected ? 1 : 0);
        
        ESP_LOGD(TAG, "Network: %s", net_status);
    }
}

// 更新场景
void bsp_nextion_update_scene(const char *scene_name, uint8_t scene_id)
{
    if (strcmp(s_state.scene_name, scene_name) != 0 || s_state.scene_id != scene_id) {
        strncpy(s_state.scene_name, scene_name, sizeof(s_state.scene_name) - 1);
        s_state.scene_name[sizeof(s_state.scene_name) - 1] = 0;
        s_state.scene_id = scene_id;
        
        _update_text_component("p0.t4", scene_name);
        _update_num_component("p0.n1", scene_id);
        
        ESP_LOGD(TAG, "Scene: %s (#%d)", scene_name, scene_id);
    }
}

// 更新鼓点节拍可视化
void bsp_nextion_update_beat(uint8_t beat, uint8_t bpm)
{
    if (s_state.beat != beat) {
        s_state.beat = beat;
        
        // 节拍指示器: 0=左鼓, 1=右鼓, 2=停止
        _update_num_component("p0.n2", beat);
        
        // 左侧LED (beat=0时亮)
        _update_num_component("p0.n3", beat == 0 ? 1 : 0);
        // 右侧LED (beat=1时亮)
        _update_num_component("p0.n4", beat == 1 ? 1 : 0);
        
        ESP_LOGD(TAG, "Beat: %s", beat == 0 ? "LEFT" : (beat == 1 ? "RIGHT" : "STOP"));
    }
    
    if (s_state.bpm != bpm) {
        s_state.bpm = bpm;
        _update_num_component("p0.n5", bpm);
        ESP_LOGD(TAG, "BPM: %d", bpm);
    }
}

// 更新BPM
void bsp_nextion_update_bpm(uint8_t bpm)
{
    if (s_state.bpm != bpm) {
        s_state.bpm = bpm;
        _update_num_component("p0.n5", bpm);
    }
}

// 刷新所有显示
void bsp_nextion_refresh(void)
{
    _update_text_component("p0.t1", s_state.running ? "Running" : "Stopped");
    _update_text_component("p0.t2", MODE_NAMES[s_state.mode < MODE_NAMES_COUNT ? s_state.mode : 0]);
    _update_text_component("p0.t3", s_state.network_ok ? s_state.ip_str : "Offline");
    _update_text_component("p0.t4", s_state.scene_name);
    _update_num_component("p0.n0", s_state.network_ok ? 1 : 0);
    _update_num_component("p0.n1", s_state.scene_id);
    _update_num_component("p0.n2", s_state.beat);
    _update_num_component("p0.n3", s_state.beat == 0 ? 1 : 0);
    _update_num_component("p0.n4", s_state.beat == 1 ? 1 : 0);
    _update_num_component("p0.n5", s_state.bpm);
}

// Nextion显示更新任务
static void nextion_task(void *pv)
{
    (void)pv;
    ESP_LOGI(TAG, "Nextion task started");
    
    // 安全检查：确保队列已创建
    if (s_nextion_queue == NULL) {
        ESP_LOGE(TAG, "Nextion queue is NULL, task exiting");
        vTaskDelete(NULL);
        return;
    }
    
    // 初始化后等待屏幕启动
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 首次刷新
    bsp_nextion_refresh();
    
    while (1) {
        // 等待队列消息 (实际项目中可通过队列接收更新)
        nextion_state_t new_state;
        if (xQueueReceive(s_nextion_queue, &new_state, pdMS_TO_TICKS(100)) == pdTRUE) {
            bsp_nextion_update_system(new_state.running, new_state.mode);
            bsp_nextion_update_network(new_state.network_ok, new_state.ip_str);
            bsp_nextion_update_scene(new_state.scene_name, new_state.scene_id);
            bsp_nextion_update_beat(new_state.beat, new_state.bpm);
        }
    }
}

// BSP初始化
void bsp_nextion_init(void)
{
    ESP_LOGI(TAG, "Nextion init: TX=GPIO43, RX=GPIO44, Baud=%d", NEXTION_BAUD_RATE);
    
    // UART配置
    uart_config_t uart_config = {
        .baud_rate = NEXTION_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    
    ESP_ERROR_CHECK(uart_param_config(NEXTION_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(NEXTION_UART_NUM, NEXTION_TX_GPIO, NEXTION_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(NEXTION_UART_NUM, NEXTION_BUF_SIZE, NEXTION_BUF_SIZE, 0, NULL, 0));
    
    // 创建队列（从4增到32，避免高速命令丢失）
    s_nextion_queue = xQueueCreate(32, sizeof(nextion_state_t));
    if (s_nextion_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create nextion queue");
        // 队列创建失败不影响UART和任务创建，任务会检查NULL
    }
    
    // 启动任务
    BaseType_t res = xTaskCreate(nextion_task, "nextion", 4096, NULL, 3, NULL);
    if (res != pdTRUE) {
        ESP_LOGE(TAG, "Failed to create nextion task");
    }
    
    ESP_LOGI(TAG, "Nextion initialized");
}
