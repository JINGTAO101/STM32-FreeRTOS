#ifndef __SG90_H
#define __SG90_H

#include "stm32f1xx_hal.h"

/************************ SG90引脚和定时器配置 *************************/
// 使用TIM2_CH3 (PA2) 输出PWM信号
#define SG90_TIM              TIM2
#define SG90_TIM_CHANNEL     TIM_CHANNEL_3
#define SG90_TIM_HANDLE     htim2

/************************ SG90参数定义 *************************/
// SG90舵机角度范围：0°-180°
#define SG90_ANGLE_MIN      0    // 最小角度
#define SG90_ANGLE_MAX      180  // 最大角度

// PWM参数：20ms周期(50Hz)，高电平0.5ms-2.5ms
#define SG90_PWM_FREQ       50    // PWM频率50Hz (周期20ms)
#define SG90_PULSE_MIN     500   // 0.5ms (对应0°)
#define SG90_PULSE_MAX     2500  // 2.5ms (对应180°)

/************************ 对外接口 *************************/
// 初始化SG90舵机
void sg90_init(void);

// 设置SG90角度 (0-180°)
// angle: 角度值 (0-180)
void sg90_set_angle(uint8_t angle);

#endif
