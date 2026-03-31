#include "led.h"

/*
 * 函数名：led_init
 * 功能：LED引脚初始化（CubeMX已经生成了GPIO初始化，这里做二次封装，方便管理）
 * 参数：无
 * 返回值：无
 */
void led_init(void)
{
    // 初始化后默认关闭LED
    LED_ON();          // 点亮LED
    HAL_Delay(500);    // 延时500ms（这里用HAL库的延时，后续接入FreeRTOS后换vTaskDelay）
    LED_OFF();         // 熄灭LED
    HAL_Delay(500);    // 延时500ms
}

/*
 * 函数名：led_on
 * 功能：点亮LED
 * 参数：无
 * 返回值：无
 */
void led_on(void)
{
    LED_ON();
}

/*
 * 函数名：led_off
 * 功能：熄灭LED
 * 参数：无
 * 返回值：无
 */
void led_off(void)
{
    LED_OFF();
}

/*
 * 函数名：led_toggle
 * 功能：翻转LED状态（亮→灭，灭→亮）
 * 参数：无
 * 返回值：无
 */
void led_toggle(void)
{
    LED_TOGGLE();
}

