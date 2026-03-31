/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "led.h"
#include "key.h"
#include "dht11.h"
#include "OLED.h"
#include "jdy31.h"
#include "sg90.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

typedef struct {
  float temperature;
  float humidity;
  uint8_t led_state;
  uint8_t sg90_angle;
} Display_Data_Typedef;

// 对应项目文档的全局设备状态结构体，紧凑对齐节省内存
typedef struct __attribute__((packed)) {
    float temperature;    // 温度值
    float humidity;       // 湿度值
    uint8_t led_state;    // LED状态：0-灭 1-亮 2-闪烁中
    uint8_t sg90_angle;   // SG90当前角度（0-180°）
    uint8_t oled_page;    // OLED当前画面号：0-温湿度画面 1-状态画面
} DeviceStatus_Typedef;

// 全局设备状态变量，仅通过互斥锁访问
DeviceStatus_Typedef g_device_status = {0};

/************************* 蓝牙通信协议定义 *************************/
// 协议帧定义：帧头(0xAA) + 指令码(1字节) + 数据段(1字节) + 校验和(1字节) + 帧尾(0x55) = 5字节
#define PROTO_FRAME_HEAD    0xAA    // 帧头
#define PROTO_FRAME_TAIL    0x55    // 帧尾
#define PROTO_DATA_SIZE     1       // 数据段大小（角度值0-180°，1字节足够）
#define PROTO_FRAME_SIZE    5       // 完整帧大小

// 指令码定义（手机APP→设备，下行控制指令）
#define CMD_LED_ON          0x01    // 点亮LED
#define CMD_LED_OFF         0x02    // 熄灭LED
#define CMD_LED_BLINK       0x03    // LED闪烁
#define CMD_SG90_SET_ANGLE  0x04    // 设置SG90角度
#define CMD_GET_STATUS      0x05    // 查询设备状态

// 指令码定义（设备→手机APP，上行数据指令）
#define CMD_REPORT_STATUS   0x10    // 上报设备状态
#define CMD_REPORT_SENSOR   0x11    // 上报温湿度数据
#define CMD_ACK             0xFF    // 应答指令

// 下行控制指令结构体（推送到Ctrl_Cmd_Queue）
typedef struct __attribute__((packed)) {
    uint8_t cmd_code;       // 指令码
    uint8_t param[PROTO_DATA_SIZE];  // 数据段
} Ctrl_Cmd_Typedef;

// 上行数据报文结构体（推送到BT_Tx_Queue）
typedef struct __attribute__((packed)) {
    uint8_t cmd_code;       // 指令码
    uint8_t data[PROTO_DATA_SIZE];   // 数据段
} 


BT_Tx_Msg_Typedef;

/* USER CODE END Variables */
osThreadId BT_Comm_TaskHandle;
osThreadId Ctrl_Exec_TaskHandle;
osThreadId Data_Sample_TasHandle;
osThreadId Key_OLED_TaskHandle;
osMessageQId BT_Rx_QueueHandle;
osMessageQId Ctrl_Cmd_QueueHandle;
osMessageQId BT_Tx_QueueHandle;
QueueHandle_t Display_Data_QueueHandle;
osMutexId DeviceStatus_MutexHandle;
osSemaphoreId Key_Event_SemHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartBTCommTask(void const * argument);
void StartCtrlExecTask(void const * argument);
void StartDataSampleTask(void const * argument);
void StartOLEDTask(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* definition and creation of DeviceStatus_Mutex */
  osMutexDef(DeviceStatus_Mutex);
  DeviceStatus_MutexHandle = osMutexCreate(osMutex(DeviceStatus_Mutex));

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* definition and creation of Key_Event_Sem */
  osSemaphoreDef(Key_Event_Sem);
  Key_Event_SemHandle = osSemaphoreCreate(osSemaphore(Key_Event_Sem), 1);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* definition and creation of BT_Rx_Queue */
  osMessageQDef(BT_Rx_Queue, 3, uint16_t);
  BT_Rx_QueueHandle = osMessageCreate(osMessageQ(BT_Rx_Queue), NULL);

  /* definition and creation of Ctrl_Cmd_Queue */
  osMessageQDef(Ctrl_Cmd_Queue, 3, uint32_t);
  Ctrl_Cmd_QueueHandle = osMessageCreate(osMessageQ(Ctrl_Cmd_Queue), NULL);

  /* definition and creation of BT_Tx_Queue */
  osMessageQDef(BT_Tx_Queue, 3, 16);
  BT_Tx_QueueHandle = osMessageCreate(osMessageQ(BT_Tx_Queue), NULL);

  /* definition and creation of Display_Data_Queue (by value, FreeRTOS queue) */
  Display_Data_QueueHandle = xQueueCreate(2, sizeof(Display_Data_Typedef));

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of BT_Comm_Task */
  osThreadDef(BT_Comm_Task, StartBTCommTask, osPriorityHigh, 0, 196);
  BT_Comm_TaskHandle = osThreadCreate(osThread(BT_Comm_Task), NULL);

  /* definition and creation of Ctrl_Exec_Task */
  osThreadDef(Ctrl_Exec_Task, StartCtrlExecTask, osPriorityAboveNormal, 0, 128);
  Ctrl_Exec_TaskHandle = osThreadCreate(osThread(Ctrl_Exec_Task), NULL);

  /* definition and creation of Data_Sample_Tas */
  osThreadDef(Data_Sample_Tas, StartDataSampleTask, osPriorityNormal, 0, 128);
  Data_Sample_TasHandle = osThreadCreate(osThread(Data_Sample_Tas), NULL);

  /* definition and creation of Key_OLED_Task */
  osThreadDef(Key_OLED_Task, StartOLEDTask, osPriorityBelowNormal, 0, 128);
  Key_OLED_TaskHandle = osThreadCreate(osThread(Key_OLED_Task), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartBTCommTask */
/**
  * @brief  Function implementing the BT_Comm_Task thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartBTCommTask */
void StartBTCommTask(void const * argument)
{
  /* USER CODE BEGIN StartBTCommTask */
  // 初始化JDY31驱动并设置接收队列
  JDY31_Init();
  JDY31_SetRxQueue(BT_Rx_QueueHandle);

  uint8_t *rx_buf = JDY31_GetRxBuf();
  uint16_t rx_len;
  osEvent rx_event;
  char *tx_data;  // 发送数据指针（从队列获取）
  for(;;)
  {
    /* ============ 阶段1：阻塞等待接收队列 ============ */
    // 阻塞等待USART1空闲中断推送到队列的消息（接收到的数据长度）
    // 修改：使用10ms超时而非osWaitForever，确保能定期检查发送队列
    rx_event = osMessageGet(BT_Rx_QueueHandle, pdMS_TO_TICKS(10));

    if (rx_event.status == osEventMessage)
    {
      rx_len = JDY31_GetRxLen();

      if (rx_len > 0)
      {
        /* ============ 阶段2：文本指令解析 ============ */
        // 将接收到的数据作为字符串处理（以\r\n结尾）
        rx_buf[rx_len] = '\0';  // 添加字符串结束符

        // 解析文本指令
        Ctrl_Cmd_Typedef ctrl_cmd;
        memset(ctrl_cmd.param, 0, PROTO_DATA_SIZE);
        ctrl_cmd.cmd_code = 0;

        // 比较指令字符串
        if (strncmp((char*)rx_buf, "LED_ON", 6) == 0)
        {
          ctrl_cmd.cmd_code = CMD_LED_ON;
        }
        else if (strncmp((char*)rx_buf, "LED_OFF", 7) == 0)
        {
          ctrl_cmd.cmd_code = CMD_LED_OFF;
        }
        else if (strncmp((char*)rx_buf, "LED_BLINK", 9) == 0)
        {
          ctrl_cmd.cmd_code = CMD_LED_BLINK;
        }
        else if (strncmp((char*)rx_buf, "SET_ANGLE=", 10) == 0)
        {
          // 解析角度参数：SET_ANGLE=90
          ctrl_cmd.cmd_code = CMD_SG90_SET_ANGLE;
          uint8_t angle = 0;
          // 方法1：使用sscanf解析
          // 从rx_buf[10]开始解析数字（跳过"SET_ANGLE="）
          // 例如："SET_ANGLE=90\r\n" → 从位置10开始是"90\r\n"
          if (rx_event.value.v > 10)
          {
            sscanf((char*)&rx_buf[10], "%hhu", &angle);
          }
          ctrl_cmd.param[0] = angle;  // 角度参数存储到param[0]
        }
        else if (strncmp((char*)rx_buf, "GET_STATUS", 10) == 0)
        {
          ctrl_cmd.cmd_code = CMD_GET_STATUS;
        }

        // 如果解析到有效指令，推送到控制指令队列
        if (ctrl_cmd.cmd_code != 0)
        {
          osMessagePut(Ctrl_Cmd_QueueHandle, *(uint32_t*)&ctrl_cmd, 0);
        }
        // 清空接收缓冲区
        JDY31_ClearRxBuf();
      }
    }

    /* ============ 阶段3：阻塞等待发送队列 ============ */
    // 阻塞等待发送队列中的温湿度数据（最多等待10ms）
    rx_event = osMessageGet(BT_Tx_QueueHandle, pdMS_TO_TICKS(10));

    if (rx_event.status == osEventMessage)
    {
      // 从队列获取数据指针（BT_Tx_Queue现在传递的是数据地址）
      tx_data = (char*)rx_event.value.p;

      // 直接发送纯文本数据（不经过协议帧封装）
      if (tx_data != NULL)
      {
        JDY31_DMA_Send((uint8_t*)tx_data, strlen(tx_data));
      }
    }
    else
    {
      // 发送队列为空，强制让出CPU，避免连续处理时占用过多时间
      osDelay(1);
    }
  }
  /* USER CODE END StartBTCommTask */
}

/* USER CODE BEGIN Header_StartCtrlExecTask */
/**
* @brief Function implementing the Ctrl_Exec_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartCtrlExecTask */
void StartCtrlExecTask(void const * argument)
{
  /* USER CODE BEGIN StartCtrlExecTask */
  // 初始化硬件外设
  sg90_init();  // 初始化SG90舵机（设置到90°中间位置）

  // 声明控制指令接收事件
  osEvent cmd_event;

  // 静态发送缓冲区（用于发送状态反馈消息）
  static char status_buf[64];

  // 声明switch中使用的变量
  float temp = 0.0f, humi = 0.0f;
  uint8_t led = 0, angle = 0;
  const char *led_str = NULL;
  int len = 0;

  for(;;)
  {
    // 阻塞等待控制指令队列（无限等待）
    cmd_event = osMessageGet(Ctrl_Cmd_QueueHandle, osWaitForever);

    if (cmd_event.status == osEventMessage)
    {
      // 将队列中的32位数据转换为Ctrl_Cmd_Typedef结构体
      Ctrl_Cmd_Typedef *ctrl_cmd = (Ctrl_Cmd_Typedef*)&cmd_event.value.v;

      // 根据指令码执行相应控制动作
      switch (ctrl_cmd->cmd_code)
      {
        case CMD_LED_ON:
          led_on();
          // 更新全局设备状态
          if (osMutexWait(DeviceStatus_MutexHandle, 100) == osOK)
          {
            g_device_status.led_state = 1;
            osMutexRelease(DeviceStatus_MutexHandle);
          }
          break;

        case CMD_LED_OFF:
          led_off();
          // 更新全局设备状态
          if (osMutexWait(DeviceStatus_MutexHandle, 100) == osOK)
          {
            g_device_status.led_state = 0;
            osMutexRelease(DeviceStatus_MutexHandle);
          }
          break;

        case CMD_LED_BLINK:
          // LED闪烁5次
          for (uint8_t i = 0; i < 5; i++)
          {
            led_on();
            osDelay(200);
            led_off();
            osDelay(200);
          }
          // 更新全局设备状态
          if (osMutexWait(DeviceStatus_MutexHandle, 100) == osOK)
          {
            g_device_status.led_state = 0;
            osMutexRelease(DeviceStatus_MutexHandle);
          }
          break;

        case CMD_SG90_SET_ANGLE:
          // 设置SG90角度（参数存储在param[0]中）
          sg90_set_angle(ctrl_cmd->param[0]);
          // 更新全局设备状态
          if (osMutexWait(DeviceStatus_MutexHandle, 100) == osOK)
          {
            g_device_status.sg90_angle = ctrl_cmd->param[0];
            osMutexRelease(DeviceStatus_MutexHandle);
          }
          break;

        case CMD_GET_STATUS:
          // 查询设备状态，格式化发送到蓝牙
          // 格式："LED:ON SG90:90 T:28.5C H:60.2%\r\n"
          if (osMutexWait(DeviceStatus_MutexHandle, 100) == osOK)
          {
            led = g_device_status.led_state;
            angle = g_device_status.sg90_angle;
            temp = g_device_status.temperature;
            humi = g_device_status.humidity;
            osMutexRelease(DeviceStatus_MutexHandle);
          }

          led_str = (led == 1) ? "ON" : ((led == 2) ? "BLK" : "OFF");
          len = snprintf(status_buf, sizeof(status_buf),
                        "LED:%s SG90:%d T:%.1fC H:%.1f%%\r\n",
                        led_str, angle, temp, humi);
          if (len > 0 && len <= sizeof(status_buf))
          {
            osMessagePut(BT_Tx_QueueHandle, (uint32_t)status_buf, 0);
          }
          break;

        default:
          // 未知指令，忽略
          break;
      }
    }
  }
  /* USER CODE END StartCtrlExecTask */
}

/* USER CODE BEGIN Header_StartDataSampleTask */
/**
* @brief Function implementing the Data_Sample_Tas thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDataSampleTask */
void StartDataSampleTask(void const * argument)
{
  /* USER CODE BEGIN StartDataSampleTask */
  // 结构体名适配新驱动：DHT11_Data_TypeDef
  static DHT11_Data_TypeDef dht11_data = {0};
  static Display_Data_Typedef display_msg = {0};
  static char tx_buf[32];  // 改为静态变量，避免DMA发送时内存被覆盖
  uint8_t res = 0;
  uint32_t xLastWakeTime;
  float temperature = 0.0f;
  float humidity = 0.0f;


  // 1. 等待DHT11上电稳定（必须！DHT11上电1s内不响应指令）
  osDelay(1000);
  // 2. 初始化周期基准时间
  xLastWakeTime = osKernelSysTick();

  /* Infinite loop */
  for(;;)
  {
    // 精准1s周期采集，符合项目文档"周期误差≤10ms"要求
    // 注意：osDelayUntil参数是tick数，configTICK_RATE_HZ=1000，所以1000=1秒
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1500));

    // 【修改3】读取函数名适配：DHT11_ReadData
    // 【修改4】返回值逻辑适配：新驱动返回1=成功，非1=失败（错误码）
    res = DHT11_ReadData(&dht11_data);
    if(res == 1)  // 采集+校验成功
    {
      // 拼接温湿度数值（DHT11小数部分固定为0，直接用整数部分）
      temperature = (float)dht11_data.temp_int + (float)dht11_data.temp_dec * 0.1f;
      humidity = (float)dht11_data.humi_int + (float)dht11_data.humi_dec * 0.1f;

      /************************* 互斥锁保护：更新全局设备状态 *************************/
      if(osMutexWait(DeviceStatus_MutexHandle, 100) == osOK)
      {
        g_device_status.temperature = temperature;
        g_device_status.humidity = humidity;
        display_msg.led_state = g_device_status.led_state;
        display_msg.sg90_angle = g_device_status.sg90_angle;
        osMutexRelease(DeviceStatus_MutexHandle);
      }

      display_msg.temperature = temperature;
      display_msg.humidity = humidity;

      /************************* 发送数据到显示队列（按值拷贝） *************************/
      xQueueSend(Display_Data_QueueHandle, &display_msg, 0);

      /************************* 发送数据到蓝牙发送队列（上报温湿度） *************************/
      // 将温湿度数据格式化后推送到队列（线程同步）
      // 格式："T:28.5C H:60.2%"
      int data_len = snprintf(tx_buf, sizeof(tx_buf), "T:%.1fC H:%.1f%%\r\n", temperature, humidity);
      if (data_len > 0 && data_len <= sizeof(tx_buf))
      {
        // 推送到蓝牙发送队列（传递数据地址）
        osMessagePut(BT_Tx_QueueHandle, (uint32_t)tx_buf, 0);
      }
      
      // 【新手调试用】可以在这里加串口打印，看采集到的数据
      // printf("采集成功：温度=%.1f℃，湿度=%.1f%%RH\r\n", temperature, humidity);
    }
    else
    {
      // DHT11 读取失败，发送错误信息（用于调试）
      const char *error_msg = "DHT11_READ_FAIL\r\n";
      JDY31_DMA_Send((uint8_t*)error_msg, strlen(error_msg));
    }
  }
  /* USER CODE END StartDataSampleTask */
}

/* USER CODE BEGIN Header_StartOLEDTask */
/**
* @brief Function implementing the Key_OLED_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartOLEDTask */
void StartOLEDTask(void const * argument)
{
  /* USER CODE BEGIN StartOLEDTask */
  led_init();
  key_init();
  OLED_Init();
  OLED_Clear();

  Display_Data_Typedef latest = {0};
  uint8_t has_data = 0;
  uint8_t need_refresh = 1;
  uint8_t current_page = 0;

  if(osMutexWait(DeviceStatus_MutexHandle, 100) == osOK)
  {
    current_page = g_device_status.oled_page;
    osMutexRelease(DeviceStatus_MutexHandle);
  }

  for(;;)
  {
    if(xQueueReceive(Display_Data_QueueHandle, &latest, pdMS_TO_TICKS(100)) == pdPASS)
    {
      has_data = 1;
      need_refresh = 1;
    }
    if(osSemaphoreWait(Key_Event_SemHandle, 0) == osOK)
    {
      osDelay(20);
      if(KEY_READ_PIN() == GPIO_PIN_RESET)
      {
        if(osMutexWait(DeviceStatus_MutexHandle, 100) == osOK)
        {
          g_device_status.oled_page = (g_device_status.oled_page == 0) ? 1 : 0;
          current_page = g_device_status.oled_page;
          osMutexRelease(DeviceStatus_MutexHandle);
        }
        need_refresh = 1;
      }
    }

    if(need_refresh && has_data)
    {
      char line[24];
      OLED_Clear();
      if(current_page == 0)
      {
        snprintf(line, sizeof(line), "Temp: %.1f C", latest.temperature);
        OLED_ShowString(1, 1, line);
        snprintf(line, sizeof(line), "Humi: %.1f %%", latest.humidity);
        OLED_ShowString(2, 1, line);
      }
      else
      {
        const char *led_str = "OFF";
        if(latest.led_state == 1) led_str = "ON";
        else if(latest.led_state == 2) led_str = "BLK";

        snprintf(line, sizeof(line), "LED: %s", led_str);
        OLED_ShowString(1, 1, line);
        snprintf(line, sizeof(line), "SG90: %3u", (unsigned int)latest.sg90_angle);
        OLED_ShowString(2, 1, line);
      }

      need_refresh = 0;
    }
  }
  /* USER CODE END StartOLEDTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

