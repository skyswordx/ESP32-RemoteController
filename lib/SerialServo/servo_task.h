

#ifndef SERVO_TASK_H
#define SERVO_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 舵机工作模式枚举
 */
typedef enum {
    SERVO_MODE_SERVO = 0,   // 舵机模式
    SERVO_MODE_MOTOR = 1    // 电机模式
} servo_mode_t;

/**
 * @brief 舵机负载状态枚举
 */
typedef enum {
    SERVO_LOAD_UNLOAD = 0,  // 卸载状态
    SERVO_LOAD_LOAD = 1     // 加载状态
} servo_load_state_t;

/**
 * @brief 舵机状态结构体
 */
typedef struct {
    uint8_t servo_id;           // 舵机ID
    bool is_connected;          // 连接状态
    servo_mode_t work_mode;     // 工作模式
    servo_load_state_t load_state; // 负载状态
    float current_position;     // 当前位置(角度)
    float current_speed;        // 当前速度
    int temperature;            // 温度
    float voltage;              // 电压
    uint32_t last_update_time;  // 最后更新时间(ms)
} servo_status_t;

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

/**
 * @brief 获取指定ID舵机的完整状态
 * @param servo_id 舵机ID
 * @param status 状态输出指针
 * @return true 成功，false 失败
 */
bool servo_get_status(uint8_t servo_id, servo_status_t *status);

/**
 * @brief 切换指定ID舵机的负载状态
 * @param servo_id 舵机ID
 * @param load_state 目标负载状态
 * @return true 成功，false 失败
 */
bool servo_set_load_state(uint8_t servo_id, servo_load_state_t load_state);

/**
 * @brief 切换指定ID舵机的工作模式
 * @param servo_id 舵机ID
 * @param mode 目标工作模式
 * @return true 成功，false 失败
 */
bool servo_set_work_mode(uint8_t servo_id, servo_mode_t mode);

/**
 * @brief 舵机模式下控制位移角度和时间
 * @param servo_id 舵机ID
 * @param angle 目标角度
 * @param time_ms 执行时间(ms)
 * @return true 成功，false 失败
 */
bool servo_control_position(uint8_t servo_id, float angle, uint32_t time_ms);

/**
 * @brief 电机模式下指定速度
 * @param servo_id 舵机ID
 * @param speed 目标速度 (-1000 到 1000)
 * @return true 成功，false 失败
 */
bool servo_control_speed(uint8_t servo_id, int16_t speed);

#ifdef __cplusplus
}
#endif

#endif // SERVO_TASK_H
