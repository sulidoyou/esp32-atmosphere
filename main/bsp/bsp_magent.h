#ifndef __BSP_MAGENT_H__
#define __BSP_MAGENT_H__

void bsp_magent_init(void);
void magent_fire_once(void);
void magent_fire_ch(int ch);
void magent_fire_ch_with_ms(int ch, int ms);

#endif
