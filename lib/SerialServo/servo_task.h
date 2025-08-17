

#ifndef SERVO_TASK_H
#define SERVO_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 串口舵机配置结构体
 */
typedef struct {
    int uart_num;           // UART端口号
    int rx_pin;             // RX引脚
    int tx_pin;             // TX引脚
    int baud_rate;          // 波特率
    int servo_id;           // 舵机ID
    bool enable_demo;       // 是否启用演示模式
    uint32_t demo_interval; // 演示动作间隔时间(ms)
} servo_task_config_t;

/**
 * @brief 初始化串口舵机配置
 * @param config 舵机配置结构体指针
 * @return pdPASS 成功，pdFAIL 失败
 */
BaseType_t servo_init_config(const servo_task_config_t *config);

/**
 * @brief 启动串口舵机任务
 * @return pdPASS 成功，pdFAIL 失败
 */
BaseType_t servo_start_task(void);

/**
 * @brief 停止串口舵机任务
 */
void servo_stop_task(void);

/**
 * @brief 获取舵机连接状态
 * @return true 已连接，false 未连接
 */
bool servo_is_connected(void);

/**
 * @brief 控制舵机移动到指定角度
 * @param angle 目标角度
 * @param time_ms 执行时间(ms)
 * @return true 成功，false 失败
 */
bool servo_move_to_angle(float angle, uint32_t time_ms);

/**
 * @brief 读取舵机当前位置
 * @param position 位置输出指针
 * @return true 成功，false 失败
 */
bool servo_read_position(float *position);

/**
 * @brief 读取舵机温度
 * @param temperature 温度输出指针
 * @return true 成功，false 失败
 */
bool servo_read_temperature(int *temperature);

/**
 * @brief 读取舵机电压
 * @param voltage 电压输出指针
 * @return true 成功，false 失败
 */
bool servo_read_voltage(float *voltage);

#ifdef __cplusplus
}
#endif

#endif // SERVO_TASK_H
