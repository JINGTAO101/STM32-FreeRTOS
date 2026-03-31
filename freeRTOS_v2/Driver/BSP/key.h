#ifndef __KEY_H
#define __KEY_H

#include "bsp_config.h"

/************************* 函数声明 *************************/
/*
 * 函数名：key_init
 * 功能：按键初始化（CubeMX已配置EXTI，这里做状态初始化）
 * 参数：无
 * 返回值：无
 */
void key_init(void);

#endif
