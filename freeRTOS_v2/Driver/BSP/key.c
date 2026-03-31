#include "key.h"

/*
 * 函数名：key_init
 * 功能：按键初始化
 * 参数：无
 * 返回值：无
 */
void key_init(void)
{
    // CubeMX的MX_GPIO_Init()已经把PA0配置成了EXTI0下降沿中断+内部上拉
    // 这里不需要重复配置，仅保留初始化接口，符合项目文档架构
}
