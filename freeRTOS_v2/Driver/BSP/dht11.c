#include "dht11.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

/************************ 私有宏定义 ************************/
#define DHT11_TIMEOUT  20000  // 超时阈值（防止死循环）
#define SYSTEM_CORE_CLOCK 72000000U  // STM32F103主频72MHz

/************************ 私有函数声明 ************************/
static void Soft_Delay_ms(uint32_t ms);    // 软件毫秒延时（绕开HAL时基）
static void delay_us(uint32_t us);         // 微秒延时（F103 72MHz专属）
static void set_pin_output(void);          // 配置引脚为开漏输出
static void set_pin_input(void);           // 配置引脚为上拉输入
static uint8_t read_bit(void);             // 读取单个Bit（临界区保护）
static uint8_t read_byte(void);            // 读取一个字节

/************************ 私有函数实现 ************************/
/**
 * @brief  软件毫秒延时（彻底绕开HAL时基依赖）
 * @param  ms: 延时毫秒数
 * @note   72MHz主频、Keil -O0优化下校准，若优化等级变化需调整循环次数
 */
static void Soft_Delay_ms(uint32_t ms)
{
    for(uint32_t i = 0; i < ms; i++)
    {
        for(uint32_t j = 0; j < 7200; j++)  // 7200次循环≈1ms（72MHz）
        {
            __NOP();  // 空操作，保证延时精度
        }
    }
}

/**
 * @brief  微秒级延时（STM32F103 72MHz专属）
 * @param  us: 延时微秒数
 * @note   Keil -O0优化用/5，-O1/-O2优化请改为/6或/7
 *        改为/7以适应不同编译器优化等级
 */
static void delay_us(uint32_t us)
{
    uint32_t count = us * (SYSTEM_CORE_CLOCK / 1000000U) / 7;
    while (count--)
    {
        __NOP();
    }
}

/**
 * @brief  配置DHT11引脚为开漏输出（单总线标准）
 */
static void set_pin_output(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;  // 开漏输出（单总线必需）
    GPIO_InitStruct.Pull = GPIO_PULLUP;           // 上拉电阻
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; // 高速模式
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

/**
 * @brief  配置DHT11引脚为上拉输入
 */
static void set_pin_input(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;       // 输入模式
    GPIO_InitStruct.Pull = GPIO_PULLUP;           // 上拉电阻
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

/**
 * @brief  读取DHT11的单个Bit（带FreeRTOS调度器保护）
 * @return 0/1=正常读取，0xFF=超时错误
 * @note   关调度器保护防止任务切换打乱时序，不影响中断响应
 */
static uint8_t read_bit(void)
{
    uint8_t bit = 0;
    uint16_t retry = 0;

    // 关闭调度器，防止任务切换（但允许中断响应）
    vTaskSuspendAll();

    // 1. 等待DHT11拉低总线（起始位）
    // DHT11发送每个bit时，先拉低50us
    retry = 0;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN))
    {
        if (retry++ > DHT11_TIMEOUT)
        {
            xTaskResumeAll();
            return 0xFF;
        }
    }

    // 2. 等待DHT11拉高总线
    retry = 0;
    while (!HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN))
    {
        if (retry++ > DHT11_TIMEOUT)
        {
            xTaskResumeAll();
            return 0xFF;
        }
    }

    // 3. 延时40us后判断电平（DHT11逻辑：0=高26-28us，1=高70us）
    delay_us(40);
    if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN))
    {
        bit = 1;
    }

    // 4. 等待高电平结束（准备下一个bit）
    retry = 0;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN))
    {
        if (retry++ > DHT11_TIMEOUT)
        {
            xTaskResumeAll();
            return 0xFF;
        }
    }

    // 恢复调度器
    xTaskResumeAll();

    return bit;
}

/**
 * @brief  读取DHT11的一个字节（8个Bit）
 * @return 0~0xFF=正常读取，0xFF=超时错误
 */
static uint8_t read_byte(void)
{
    uint8_t i, data = 0;
    for (i = 0; i < 8; i++)
    {
        data <<= 1;          // 左移，接收下一位
        uint8_t b = read_bit();
        if (b == 0xFF)       // 单个Bit读取超时
        {
            return 0xFF;
        }
        data |= b;           // 拼接当前Bit
    }
    return data;
}

/************************ 对外函数实现 ************************/
/**
 * @brief  读取DHT11温湿度数据
 * @param  data: 存储数据的结构体指针
 * @return 1=成功，0=失败（超时/校验和错误）
 */
uint8_t DHT11_ReadData(DHT11_Data_TypeDef *data)
{
    uint16_t retry = 0;
    uint8_t buf[5] = {0};  // 存储5字节数据：湿度整数/小数/温度整数/小数/校验和

    // 1. 主机拉低总线，复位DHT11（单总线协议）
    set_pin_output();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    Soft_Delay_ms(20);     // 拉低20ms（DHT11要求至少18ms，稍微增加以确保可靠）
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    delay_us(40);          // 主机释放总线，延时40us等待DHT11响应（原30us）

    // 2. 检测DHT11的响应信号
    set_pin_input();

    // 等待DHT11拉低总线（响应起始）
    retry = 0;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN))
    {
        if (retry++ > DHT11_TIMEOUT)
        {
            // 调试：DHT11没有拉低总线响应（步骤1失败）
            return 0x01;  // 失败代码：DHT11未响应
        }
    }
    // 等待DHT11拉高总线
    retry = 0;
    while (!HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN))
    {
        if (retry++ > DHT11_TIMEOUT)
        {
            return 0x02;  // 失败代码：DHT11拉低后未拉高
        }
    }
    // 等待DHT11再次拉低总线（准备发送数据）
    retry = 0;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN))
    {
        if (retry++ > DHT11_TIMEOUT)
        {
            return 0x03;  // 失败代码：DHT11未开始发送数据
        }
    }

    // 3. 读取5字节数据
    for (uint8_t i = 0; i < 5; i++)
    {
        buf[i] = read_byte();
        if (buf[i] == 0xFF)  // 单个字节读取超时
        {
            return 0x04;
        }
    }

    // 4. 校验和验证（核心：防止数据传输错误）
    if (buf[4] != (buf[0] + buf[1] + buf[2] + buf[3]))
    {
        return 0x05;  // 失败代码：校验和错误
    }

    // 5. 赋值给输出结构体
    data->humi_int  = buf[0];
    data->humi_dec  = buf[1];
    data->temp_int  = buf[2];
    data->temp_dec  = buf[3];
    data->check_sum = buf[4];

    return 1;  // 读取成功
}
