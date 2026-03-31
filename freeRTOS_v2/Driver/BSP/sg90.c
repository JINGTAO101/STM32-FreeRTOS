#include "sg90.h"
#include "tim.h"

/************************ 私有变量 ************************/
static uint8_t sg90_current_angle = 90;  // 当前角度（初始90°）

/************************ 私有函数声明 ************************/
static uint32_t sg90_angle_to_pulse(uint8_t angle);

/**
 * @brief  将角度转换为PWM脉宽
 * @param  angle: 角度值 (0-180)
 * @return PWM脉宽值 (us)
 * @note   0° -> 0.5ms (500us)
 *          180° -> 2.5ms (2500us)
 */
static uint32_t sg90_angle_to_pulse(uint8_t angle)
{
    // 角度限制在0-180范围内
    if (angle > SG90_ANGLE_MAX)
    {
        angle = SG90_ANGLE_MAX;
    }

    // 线性映射：角度 → 脉宽
    // pulse = 500 + (angle * 2000 / 180)
    return SG90_PULSE_MIN + ((uint32_t)angle * (SG90_PULSE_MAX - SG90_PULSE_MIN) / SG90_ANGLE_MAX);
}

/************************ 对外函数实现 ************************/
/**
 * @brief  初始化SG90舵机
 * @param  无
 * @retval 无
 * @note   CubeMX已配置TIM2_CH3，这里启动PWM并设置初始角度
 */
void sg90_init(void)
{
    // 启动TIM2_CH3的PWM输出
    HAL_TIM_PWM_Start(&SG90_TIM_HANDLE, SG90_TIM_CHANNEL);

    // 设置初始角度为90°（中间位置）
    sg90_set_angle(90);
}

/**
 * @brief  设置SG90角度
 * @param  angle: 角度值 (0-180)
 * @retval 无
 * @note   通过PWM脉宽控制舵机角度
 *         0° -> 0.5ms脉宽
 *         90° -> 1.5ms脉宽
 *         180° -> 2.5ms脉宽
 */
void sg90_set_angle(uint8_t angle)
{
    uint32_t pulse_width_us;
    uint32_t ccr_value;

    // 角度限制在0-180范围内
    if (angle > SG90_ANGLE_MAX)
    {
        angle = SG90_ANGLE_MAX;
    }

    // 保存当前角度
    sg90_current_angle = angle;

    // 将角度转换为PWM脉宽
    pulse_width_us = sg90_angle_to_pulse(angle);

    // 将脉宽转换为CCR寄存器值
    // TIM2配置：PSC=719, ARR=1999, TIM2时钟72MHz
    // PWM周期 = (PSC+1)*(ARR+1)/72000000 = 720*2000/72000000 = 0.02s = 20ms (50Hz)
    // CCR值 = pulse_width_us / 20ms * 2000 = pulse_width_us / 10
    ccr_value = pulse_width_us / 10;

    // 直接设置CCR寄存器（确保TIM2_CH3对应的CCR3）
    // 同时更新ARR以确保PWM重新加载
    TIM2->CCR3 = ccr_value;

    // 强制更新影子寄存器（某些情况需要）
    TIM2->EGR = TIM_EGR_UG;
}
