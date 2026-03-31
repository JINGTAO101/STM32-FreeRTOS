#include "jdy31.h"
#include "usart.h"
#include <string.h>
#include "FreeRTOS.h"
#include "queue.h"

/************************* 全局变量定义 *************************/
// DMA接收缓冲区（使用静态数组，避免动态内存分配）
static uint8_t jdy31_rx_buf[JDY31_RX_BUF_SIZE];

// 当前接收到的数据长度（由空闲中断更新）
static volatile uint16_t jdy31_rx_len = 0;

// 上次DMA接收位置（用于计算接收长度）
static volatile uint16_t jdy31_last_pos = 0;

// 接收队列句柄（用于在中断中推送数据长度）
static osMessageQId jdy31_rx_queue = NULL;

/************************* 函数实现 *************************/

/**
  * @brief  初始化JDY31蓝牙模块
  * @note   配置USART1 DMA接收为循环模式，使能USART1空闲中断
  * @param  None
  * @retval None
  */
void JDY31_Init(void)
{
    // 清空接收缓冲区
    memset(jdy31_rx_buf, 0, JDY31_RX_BUF_SIZE);
    jdy31_rx_len = 0;
    jdy31_last_pos = 0;

    // 启动USART1 DMA接收（循环模式）
    HAL_UART_Receive_DMA(&JDY31_UART_HANDLE, jdy31_rx_buf, JDY31_RX_BUF_SIZE);

    // 使能USART1空闲中断（用于检测一帧数据的结束）
    __HAL_UART_ENABLE_IT(&JDY31_UART_HANDLE, UART_IT_IDLE);
}

/**
  * @brief  DMA方式发送数据（非阻塞）
  * @param  pData: 数据缓冲区指针
  * @param  Size: 数据长度（字节）
  * @retval HAL状态值（HAL_OK/HAL_ERROR/HAL_BUSY）
  */
HAL_StatusTypeDef JDY31_DMA_Send(uint8_t *pData, uint16_t Size)
{
    if (pData == NULL || Size == 0)
    {
        return HAL_ERROR;
    }

    return HAL_UART_Transmit_DMA(&JDY31_UART_HANDLE, pData, Size);
}

/**
  * @brief  获取DMA接收缓冲区指针
  * @param  None
  * @retval 接收缓冲区指针
  */
uint8_t* JDY31_GetRxBuf(void)
{
    return jdy31_rx_buf;
}

/**
  * @brief  获取当前接收到的数据长度
  * @note   此值由USART1空闲中断回调更新
  * @param  None
  * @retval 接收到的数据长度
  */
uint16_t JDY31_GetRxLen(void)
{
    return jdy31_rx_len;
}

/**
  * @brief  清空接收缓冲区
  * @param  None
  * @retval None
  */
void JDY31_ClearRxBuf(void)
{
    memset(jdy31_rx_buf, 0, JDY31_RX_BUF_SIZE);
    jdy31_rx_len = 0;
    jdy31_last_pos = 0;
}

/**
  * @brief  查询发送完成状态
  * @param  None
  * @retval 1-发送完成, 0-发送中
  */
uint8_t JDY31_IsTxCplt(void)
{
    return (JDY31_UART_HANDLE.gState == HAL_UART_STATE_READY);
}

/**
  * @brief  等待发送完成（带超时）
  * @param  timeout: 超时时间（ms）
  * @retval HAL状态值（HAL_OK/HAL_TIMEOUT）
  */
HAL_StatusTypeDef JDY31_WaitTxCplt(uint32_t timeout)
{
    uint32_t tickstart = HAL_GetTick();

    while ((JDY31_UART_HANDLE.gState != HAL_UART_STATE_READY))
    {
        if ((HAL_GetTick() - tickstart) >= timeout)
        {
            return HAL_TIMEOUT;
        }
    }

    return HAL_OK;
}

/**
  * @brief  设置接收队列句柄
  * @param  queue: 接收队列句柄
  * @retval None
  */
void JDY31_SetRxQueue(osMessageQId queue)
{
    jdy31_rx_queue = (QueueHandle_t)queue;
}

/**
  * @brief  USART1空闲中断回调函数
  * @note   当检测到空闲中断时，计算接收到的数据长度并推送到队列
  * @param  huart: UART句柄指针
  * @retval None
  */
void HAL_UART_IdleCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == JDY31_UART)
    {
        // 停止DMA接收（获取当前接收位置）
        HAL_UART_DMAStop(&JDY31_UART_HANDLE);

        // 计算已接收的数据长度
        uint32_t current_pos = JDY31_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(JDY31_UART_HANDLE.hdmarx);

        // 处理循环缓冲区回绕情况
        if (current_pos >= jdy31_last_pos)
        {
            // 正常情况：DMA顺序写入
            jdy31_rx_len = current_pos - jdy31_last_pos;
        }
        else
        {
            // 回绕情况：DMA从缓冲区末尾跳到开头
            // 计算两段数据的总长度：(缓冲区末尾 - 上次位置) + (当前位置)
            jdy31_rx_len = (JDY31_RX_BUF_SIZE - jdy31_last_pos) + current_pos;
        }

        // 如果数据长度超过缓冲区大小，说明丢失数据（缓冲区溢出）
        if (jdy31_rx_len > JDY31_RX_BUF_SIZE)
        {
            jdy31_rx_len = JDY31_RX_BUF_SIZE;
        }

        // 更新上次位置（循环模式下）
        jdy31_last_pos = current_pos;

        // 重新启动DMA接收
        HAL_UART_Receive_DMA(&JDY31_UART_HANDLE, jdy31_rx_buf, JDY31_RX_BUF_SIZE);

        // 将接收到的数据长度推送到队列（通知任务处理）- 使用FromISR版本
        if (jdy31_rx_queue != NULL && jdy31_rx_len > 0)
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            uint16_t rx_len_copy = jdy31_rx_len;  // 创建临时变量，避免volatile类型不兼容
            xQueueSendFromISR(jdy31_rx_queue, &rx_len_copy, &xHigherPriorityTaskWoken);

            // 如果有更高优先级任务被唤醒，请求任务切换
            if (xHigherPriorityTaskWoken != pdFALSE)
            {
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
        }
    }
}

/**
  * @brief  在stm32f1xx_it.c的USART1_IRQHandler中调用此函数
  * @note   用于处理USART1空闲中断
  * @retval None
  */
void JDY31_IRQHandler(void)
{
    uint32_t isrflags = READ_REG(JDY31_UART_HANDLE.Instance->SR);
    uint32_t cr1its = READ_REG(JDY31_UART_HANDLE.Instance->CR1);

    // 检测到空闲中断标志
    if ((isrflags & USART_SR_IDLE) != RESET)
    {
        // 清除空闲中断标志（读DR寄存器）
        __HAL_UART_CLEAR_IDLEFLAG(&JDY31_UART_HANDLE);

        // 调用空闲中断回调
        HAL_UART_IdleCallback(&JDY31_UART_HANDLE);
    }
}
