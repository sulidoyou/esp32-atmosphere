
#include "bsp.h"
#define TAG "PCA9557"
static i2c_master_dev_handle_t pca9557_dev_handle = NULL;


// 读取PCA9557寄存器的值
esp_err_t pca9557_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    esp_err_t ret = i2c_master_transmit(pca9557_dev_handle, &reg_addr, 1, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK) {
        return ret;
    }
    return i2c_master_receive(pca9557_dev_handle, data, len, pdMS_TO_TICKS(1000));
}

// 给PCA9557的寄存器写值
esp_err_t pca9557_register_write_byte(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};

    return i2c_master_transmit(pca9557_dev_handle, write_buf, sizeof(write_buf), pdMS_TO_TICKS(1000));
}


void bsp_pca9557_init(void)
{
    int ret = 0;
    i2c_device_config_t pca9557_dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_7,  // 7位I2C地址
        .device_address = 0x19, // PCA9557地址
        .scl_speed_hz = 100000, // 通信频率
    };


    ret = i2c_master_bus_add_device(i2c_bus_handle, &pca9557_dev_config, &pca9557_dev_handle);
    if (ret != ESP_OK) {
        printf("PCA9557 i2c_master_bus_add_device failed, ret=%d\n", ret);
        return;  // ✅ 不崩溃，继续运行
    }
    printf("PCA9557 init success\n");

    // 写入控制引脚默认值 DVP_PWDN=1  PA_EN = 0  LCD_CS = 1
    ret = pca9557_register_write_byte(PCA9557_OUTPUT_PORT, 0x05);
    if(ret == ESP_OK) {
        printf("PCA9557 write output port success\n");
    } else {
        printf("PCA9557 write output port failed (device may not be present)\n");
        // ✅ 不崩溃，PA_EN将保持默认关闭，音频功放可能没声音（但不会死机）
    }

    // 把PCA9557芯片的IO1 IO1 IO2设置为输出 其它引脚保持默认的输入
    ret = pca9557_register_write_byte(PCA9557_CONFIGURATION_PORT, 0xf8);
    if(ret == ESP_OK) {
        printf("PCA9557 write configuration port success\n");
    } else {
        printf("PCA9557 write configuration port failed\n");
    }
}




// 设置PCA9557芯片的某个IO引脚输出高低电平
esp_err_t pca9557_set_output_state(uint8_t gpio_bit, uint8_t level)
{
    uint8_t data;
    esp_err_t res = ESP_FAIL;

    pca9557_register_read(PCA9557_OUTPUT_PORT, &data, 1);
    res = pca9557_register_write_byte(PCA9557_OUTPUT_PORT, SET_BITS(data, gpio_bit, level));

    return res;
}

// 控制 PCA9557_LCD_CS 引脚输出高低电平 参数0输出低电平 参数1输出高电平 
void lcd_cs(uint8_t level)
{
    pca9557_set_output_state(LCD_CS_GPIO, level);
}

// 控制 PCA9557_PA_EN 引脚输出高低电平 参数0输出低电平 参数1输出高电平
void pa_en(uint8_t level)
{
    pca9557_set_output_state(PA_EN_GPIO, level);  // ✅ 已启用
}

// 控制 PCA9557_DVP_PWDN 引脚输出高低电平 参数0输出低电平 参数1输出高电平 
void dvp_pwdn(uint8_t level)
{
    //pca9557_set_output_state(DVP_PWDN_GPIO, level);
}

/***************    IO扩展芯片 ↑   *************************/
/***********************************************************/





