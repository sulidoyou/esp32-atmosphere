#include "bsp.h"

i2c_master_bus_handle_t i2c_bus_handle;


#define I2C_MASTER_SCL_IO GPIO_NUM_2
#define I2C_MASTER_SDA_IO GPIO_NUM_1



void bsp_i2c_init(void)
{
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle));


}



















