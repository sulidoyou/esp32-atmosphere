
#ifndef __BSP_H__
#define __BSP_H__ 

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/i2s_tdm.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"




#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include <locale.h>  // 区域设置库，用于支持中文


#include "version.h"

#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "audio_codec_ctrl_if.h"
#include "audio_codec_gpio_if.h"
#include "es8311_codec.h"
#include "file_iterator.h"
#include "audio_player.h"

#include "bsp_i2c.h"
#include "bsp_i2s.h"
#include "bsp_es8311.h"
#include "bsp_pca9557.h"
#include "bsp_tf.h"
#include "bsp_music_play.h"
#include "bsp_led.h"
#include "bsp_magent.h"
#include "bsp_drum.h"
#include "bsp_breathing_led.h"
#include "bsp_key.h"
#include "bsp_keyhandle.h"
#include "bsp_adc.h"
#include "bsp_mic.h"
#include "bsp_wifi.h"

void bsp_init(void);



#endif












