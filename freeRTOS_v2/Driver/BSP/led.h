#ifndef __LED_H
#define __LED_H

#include "bsp_config.h"

// 函数声明：LED初始化
void led_init(void);

// 函数声明：LED控制
void led_on(void);       // 点亮LED
void led_off(void);      // 熄灭LED
void led_toggle(void);    // 翻转LED状态

#endif

