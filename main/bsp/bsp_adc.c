#include "bsp.h"

// 日志标签
#define TAG "ADC_READ"

// 引脚映射：ESP32-S3 GPIO35=ADC1_CH7，GPIO36=ADC1_CH8
#define ADC35_CHANNEL    ADC1_CHANNEL_7
#define ADC36_CHANNEL    ADC1_CHANNEL_8
// ADC 采样参数
#define ADC_WIDTH        ADC_WIDTH_BIT_12  // 12位精度（0~4095）
#define ADC_ATTEN        ADC_ATTEN_DB_11   // 11dB衰减，支持0~3.3V输入
#define SAMPLE_COUNT     10                // 多次采样取平均，防抖

// ADC 校准句柄
static esp_adc_cal_characteristics_t adc_chars;

/**
 * @brief 初始化 ADC（GPIO35/36），启用校准
 */
void adc35_36_init(void) {
    // 1. 检查校准值（ESP32-S3 优先使用 eFuse 校准）
    esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF);
    // 2. 初始化 ADC 校准特性
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH, 0, &adc_chars);

    // 3. 配置 ADC1 通道（GPIO35）
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC35_CHANNEL, ADC_ATTEN);
    // 4. 配置 ADC1 通道（GPIO36）
    adc1_config_channel_atten(ADC36_CHANNEL, ADC_ATTEN);

    ESP_LOGI(TAG, "GPIO35/GPIO36 ADC 初始化完成（0~3.3V 范围）");
}

/**
 * @brief 读取单个 ADC 通道电压（多次采样平均，提升精度）
 * @param channel ADC1 通道号（如 ADC35_CHANNEL）
 * @return 电压值（单位：V，保留3位小数）
 */
float adc_read_voltage(adc1_channel_t channel) {
    uint32_t adc_sum = 0;
    // 多次采样取平均，防抖
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        adc_sum += adc1_get_raw(channel);
        vTaskDelay(pdMS_TO_TICKS(1));  // 短延时，避免采样过快
    }
    uint32_t adc_avg = adc_sum / SAMPLE_COUNT;

    // 用校准参数转换为实际电压（mV）→ 转V
    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(adc_avg, &adc_chars);
    float voltage_v = voltage_mv / 1000.0f;

    return voltage_v;
}

/**
 * @brief 循环读取 GPIO35/36 电压的任务
 */
void adc_read_task(void *pvParameters) {
    while (1) {
        // 读取 GPIO35 电压
        float volt35 = adc_read_voltage(ADC35_CHANNEL);
        // 读取 GPIO36 电压
        float volt36 = adc_read_voltage(ADC36_CHANNEL);

        // 打印结果（保留3位小数，直观）
        ESP_LOGI(TAG, "GPIO35 电压: %.3f V | GPIO36 电压: %.3f V", volt35, volt36);

        // 每1秒读取一次，避免日志刷屏
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelete(NULL);
}

void bsp_adc_init(void) 
{
    // 初始化 ADC
    adc35_36_init();

    // 创建 ADC 读取任务（非阻塞，避免看门狗复位）
    xTaskCreate(
        adc_read_task,  // 任务函数
        "adc_read",     // 任务名称
        2048,           // 栈大小（IDF5.3 中 2048 足够）
        NULL,           // 任务参数
        5,              // 任务优先级
        NULL            // 任务句柄
    );
    ESP_LOGI(TAG, "ADC 读取任务已启动");
}
