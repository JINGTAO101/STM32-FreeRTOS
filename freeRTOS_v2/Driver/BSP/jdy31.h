#ifndef __JDY31_H
#define __JDY31_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include "cmsis_os.h"

/************************* JDY31 硬件配置 *************************/
#define JDY31_UART          USART1
#define JDY31_UART_HANDLE   huart1
#define JDY31_BAUDRATE      9600  // JDY31模块默认波特率

/************************* 接收缓冲区配置 *************************/
#define JDY31_RX_BUF_SIZE   32    // DMA接收缓冲区大小（最大指令13字节）

/************************* 函数声明 *************************/
// 初始化JDY31模块
void JDY31_Init(void);
	
/* 外部函数声明 */
void JDY31_IRQHandler(void);

// DMA发送数据（非阻塞）
// pData: 数据缓冲区指针
// Size: 数据长度（字节）
// 返回值: HAL_OK-发送成功, HAL_ERROR-发送失败
HAL_StatusTypeDef JDY31_DMA_Send(uint8_t *pData, uint16_t Size);

// 获取DMA接收缓冲区指针
uint8_t* JDY31_GetRxBuf(void);

// 获取当前接收到的数据长度
// 注意：需要在空闲中断回调中维护接收长度
uint16_t JDY31_GetRxLen(void);

// 清空接收缓冲区
void JDY31_ClearRxBuf(void);

// 查询发送完成状态
// 返回值: 1-发送完成, 0-发送中
uint8_t JDY31_IsTxCplt(void);

// 等待发送完成（带超时）
// timeout: 超时时间（ms）
// 返回值: HAL_OK-发送完成, HAL_TIMEOUT-超时
HAL_StatusTypeDef JDY31_WaitTxCplt(uint32_t timeout);

// 设置接收队列句柄（用于在中断中推送数据）
// queue: 接收队列句柄
void JDY31_SetRxQueue(osMessageQId queue);

#ifdef __cplusplus
}
#endif

#endif
