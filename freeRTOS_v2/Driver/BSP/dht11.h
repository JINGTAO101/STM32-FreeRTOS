#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f1xx_hal.h"
#include "stdint.h"

/************************ 可配置参数 ************************/
// DHT11连接的GPIO端口和引脚（使用main.h中定义的宏）
#define DHT11_PORT    DHT11_GPIO_Port
#define DHT11_PIN     DHT11_Pin

/************************ 数据结构体 ************************/
// DHT11温湿度数据存储结构体
typedef struct {
    uint8_t humi_int;   // 湿度整数部分 (0~99)
    uint8_t humi_dec;   // 湿度小数部分 (DHT11固定为0)
    uint8_t temp_int;   // 温度整数部分 (0~50)
    uint8_t temp_dec;   // 温度小数部分 (DHT11固定为0)
    uint8_t check_sum;  // 校验和 (humi_int+humi_dec+temp_int+temp_dec)
} DHT11_Data_TypeDef;

/************************ 对外接口 ************************/
// 功能：读取DHT11温湿度数据
// 参数：data - 存储温湿度数据的结构体指针
// 返回：1=读取成功，0=读取失败（超时/校验和错误）
uint8_t DHT11_ReadData(DHT11_Data_TypeDef *data);

#endif /* __DHT11_H */
