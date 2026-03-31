#ifndef __BSP_CONFIG_H
#define __BSP_CONFIG_H

// 包含STM32 HAL库头文件，所有驱动都要用到
#include "stm32f1xx_hal.h"

/************************* LED引脚定义 *************************/
// 对应你CubeMX里配置的PC13引脚
#define LED_GPIO_PORT    GPIOC
#define LED_GPIO_PIN     GPIO_PIN_13

// 封装LED操作宏，后续代码直接调用，不用写HAL库函数
// 注意：STM32F103核心板的PC13板载LED，一般是低电平点亮，高电平熄灭
#define LED_ON()         HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET)
#define LED_OFF()        HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_SET)
#define LED_TOGGLE()     HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN)

/************************* 按键硬件配置 *************************/
// 对应CubeMX里配置的PA0引脚
#define KEY_GPIO_PORT    GPIOA
#define KEY_GPIO_PIN     GPIO_PIN_0

// 读取按键状态（按下时PA0是低电平）
#define KEY_READ_PIN()   HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY_GPIO_PIN)

/************************* DHT11 硬件配置 *************************/
// 对应CubeMX里配置的PB12引脚
#define DHT11_GPIO_PORT    GPIOB
#define DHT11_GPIO_PIN     GPIO_PIN_12

/************************* DHT11 单总线操作宏 *************************/
// 切换PB12为推挽输出模式（发送起始信号）
#define DHT11_SET_OUTPUT()  {GPIOB->CRH &= ~(0xF << 16); GPIOB->CRH |= (0x3 << 16);}
// 切换PB12为上拉输入模式（接收DHT11数据）
#define DHT11_SET_INPUT()   {GPIOB->CRH &= ~(0xF << 16); GPIOB->CRH |= (0x8 << 16);}

// 总线高低电平操作
#define DHT11_WRITE_HIGH()  HAL_GPIO_WritePin(DHT11_GPIO_PORT, DHT11_GPIO_PIN, GPIO_PIN_SET)
#define DHT11_WRITE_LOW()   HAL_GPIO_WritePin(DHT11_GPIO_PORT, DHT11_GPIO_PIN, GPIO_PIN_RESET)
// 读取总线电平
#define DHT11_READ_PIN()    HAL_GPIO_ReadPin(DHT11_GPIO_PORT, DHT11_GPIO_PIN)

// task_config.h
/* OLED显示任务配置 */
#define OLED_TASK_PRIO         1       // 最低优先级（符合项目任务优先级规划）
#define OLED_TASK_STACK_SIZE   192     // 栈大小192Word=768字节（实测足够）
#define OLED_QUEUE_LEN         2       // 显示数据队列长度（项目规划值）
#define OLED_QUEUE_ITEM_SIZE   sizeof(Display_Data_Typedef) // 队列项大小


#endif
