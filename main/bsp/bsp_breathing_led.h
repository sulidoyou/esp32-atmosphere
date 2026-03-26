#ifndef __BSP_BREATHING_LED_H__
#define __BSP_BREATHING_LED_H__

#include <stdint.h>

#define BREATHING_LED_MODE_BREATH    0
#define BREATHING_LED_MODE_RAINBOW   1

void bsp_breathing_led_init(void);
void breathing_led_set_mode(uint8_t mode);
uint8_t breathing_led_get_mode(void);
void breathing_led_flash(uint16_t ms);

#endif
