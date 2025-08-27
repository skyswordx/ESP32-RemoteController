/**
 * @file gripper_controller.h
 * @brief 智能夹爪控制系统头文件 - 基于PID控制器和斜坡规划器实现丝滑精密控制
 * @version 1.0
 * @date 2025-08-27
 * @author GitHub Copilot
 * 
 * @copyright Copyright (c) 2025 SENTRY Team
 * 
 * @note 实现夹爪控制系统指导文档中描述的所有功能：
 *       - 丝滑百分比控制（斜坡规划器）
 *       - 精密反馈控制（PID控制器）
 *       - 实时位置读取和状态管理
 *       - 阻力补偿和机械死区处理
 *       - 现场校准和参数持久化
 */

#ifndef GRIPPER_CONTROLLER_H
#define GRIPPER_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Constants ----------------------------------------------------------------*/
#define MAX_GRIPPERS                    4       ///< 最大支持的夹爪数量
#define GRIPPER_CONTROL_FREQUENCY_HZ    20      ///< 控制频率（20Hz = 50ms周期）
#define GRIPPER_FEEDBACK_FREQUENCY_HZ   10      ///< 反馈读取频率（10Hz = 100ms周期）
#define GRIPPER_DEFAULT_CLOSED_ANGLE    160.0f  ///< 默认闭合角度
#define GRIPPER_DEFAULT_OPEN_ANGLE      90.0f   ///< 默认张开角度
#define GRIPPER_DEFAULT_MIN_STEP        5.0f    ///< 默认最小步长
#define GRIPPER_CONTROL_PRECISION       0.5f    ///< 控制精度（百分比）

/* Enumerations -------------------------------------------------------------*/

/**
 * @brief 夹爪控制状态枚举
 */
typedef enum {
    GRIPPER_STATE_IDLE = 0,         ///< 空闲状态
    GRIPPER_STATE_MOVING,           ///< 运动中
    GRIPPER_STATE_HOLDING,          ///< 保持位置
    GRIPPER_STATE_ERROR,            ///< 错误状态
    GRIPPER_STATE_CALIBRATING       ///< 校准中
} gripper_state_e;

/**
 * @brief 夹爪控制模式枚举
 */
typedef enum {
    GRIPPER_MODE_OPEN_LOOP = 0,     ///< 开环控制（斜坡规划）
    GRIPPER_MODE_CLOSED_LOOP,       ///< 闭环控制（PID + 斜坡规划）
    GRIPPER_MODE_FORCE_CONTROL      ///< 力控制模式（预留）
} gripper_mode_e;

/* Structures ---------------------------------------------------------------*/

/**
 * @brief 夹爪角度映射配置结构体
 */
typedef struct {
    float closed_angle;             ///< 闭合状态角度 (度)
    float open_angle;               ///< 张开状态角度 (度)
    float min_step;                 ///< 最小有效步长 (度)
    float max_speed;                ///< 最大运动速度 (%/s)
    bool is_calibrated;             ///< 是否已校准
    bool reverse_direction;         ///< 是否反向映射
} gripper_mapping_t;

/**
 * @brief 夹爪状态结构体
 */
typedef struct {
    uint8_t servo_id;               ///< 舵机ID
    gripper_state_e state;          ///< 当前状态
    gripper_mode_e mode;            ///< 控制模式
    
    /* 位置信息 */
    float current_percent;          ///< 当前百分比位置 (0-100)
    float target_percent;           ///< 目标百分比位置 (0-100)
    float current_angle;            ///< 当前对应角度 (度)
    float hardware_angle;           ///< 实时读取的码盘值 (度)
    
    /* 运动状态 */
    bool is_moving;                 ///< 是否正在运动
    float movement_progress;        ///< 运动进度 (0-100%)
    uint32_t movement_start_time;   ///< 运动开始时间 (ms)
    uint32_t movement_duration;     ///< 预计运动时间 (ms)
    
    /* 反馈状态 */
    bool feedback_valid;            ///< 反馈是否有效
    uint32_t last_feedback_time;    ///< 上次反馈时间 (ms)
    float position_error;           ///< 位置误差 (%)
    
    /* 统计信息 */
    uint32_t total_movements;       ///< 总运动次数
    float max_position_error;       ///< 最大位置误差
    uint32_t last_update_time;      ///< 最后更新时间 (ms)
} gripper_status_t;

/**
 * @brief 夹爪控制参数结构体
 */
typedef struct {
    /* 斜坡规划参数 */
    float slope_increase_rate;      ///< 上升斜率 (%/周期)
    float slope_decrease_rate;      ///< 下降斜率 (%/周期)
    bool slope_real_first;          ///< 真实值优先使能
    
    /* PID控制参数 */
    float pid_kp;                   ///< PID比例增益
    float pid_ki;                   ///< PID积分增益
    float pid_kd;                   ///< PID微分增益
    float pid_output_limit;         ///< PID输出限制
    float pid_dead_zone;            ///< PID死区
    
    /* 死区处理参数 */
    float static_friction_compensation; ///< 静摩擦补偿
    float dynamic_friction_coeff;       ///< 动摩擦系数
    float backlash_compensation;        ///< 间隙补偿
    
    /* 安全参数 */
    float max_position_error;       ///< 最大允许位置误差 (%)
    uint32_t feedback_timeout_ms;   ///< 反馈超时时间 (ms)
    uint32_t safety_stop_timeout;   ///< 安全停止超时 (ms)
} gripper_control_params_t;

/* Function declarations ----------------------------------------------------*/

/**
 * @brief 初始化夹爪控制系统
 * @return true 成功，false 失败
 */
bool gripper_controller_init(void);

/**
 * @brief 反初始化夹爪控制系统
 */
void gripper_controller_deinit(void);

/**
 * @brief 配置夹爪映射参数
 * @param servo_id 舵机ID
 * @param mapping 映射参数结构体指针
 * @return true 成功，false 失败
 */
bool gripper_configure_mapping(uint8_t servo_id, const gripper_mapping_t *mapping);

/**
 * @brief 设置夹爪控制参数
 * @param servo_id 舵机ID
 * @param params 控制参数结构体指针
 * @return true 成功，false 失败
 */
bool gripper_set_control_params(uint8_t servo_id, const gripper_control_params_t *params);

/**
 * @brief 设置夹爪控制模式
 * @param servo_id 舵机ID
 * @param mode 控制模式
 * @return true 成功，false 失败
 */
bool gripper_set_mode(uint8_t servo_id, gripper_mode_e mode);

/**
 * @brief 丝滑夹爪控制 - 主要控制接口
 * @param servo_id 舵机ID
 * @param target_percent 目标百分比 (0-100, 0=闭合, 100=张开)
 * @param time_ms 期望运动时间 (ms, 0=使用默认速度)
 * @return true 成功，false 失败
 */
bool gripper_control_smooth(uint8_t servo_id, float target_percent, uint32_t time_ms);

/**
 * @brief 立即停止夹爪运动
 * @param servo_id 舵机ID
 * @return true 成功，false 失败
 */
bool gripper_stop(uint8_t servo_id);

/**
 * @brief 获取夹爪当前百分比位置
 * @param servo_id 舵机ID
 * @param current_percent 输出当前百分比指针
 * @return true 成功，false 失败
 */
bool gripper_get_current_percent(uint8_t servo_id, float *current_percent);

/**
 * @brief 获取夹爪完整状态
 * @param servo_id 舵机ID
 * @param status 状态输出指针
 * @return true 成功，false 失败
 */
bool gripper_get_status(uint8_t servo_id, gripper_status_t *status);

/**
 * @brief 夹爪现场校准 - 设置当前位置为参考点
 * @param servo_id 舵机ID
 * @param reference_position 参考位置 ("closed", "open", 或百分比数值)
 * @return true 成功，false 失败
 */
bool gripper_calibrate_position(uint8_t servo_id, const char *reference_position);

/**
 * @brief 夹爪映射参数微调
 * @param servo_id 舵机ID
 * @param position_type 位置类型 ("closed" 或 "open")
 * @param angle_offset 角度偏移 (度)
 * @return true 成功，false 失败
 */
bool gripper_adjust_mapping(uint8_t servo_id, const char *position_type, float angle_offset);

/**
 * @brief 保存夹爪配置到非易失性存储
 * @param servo_id 舵机ID
 * @return true 成功，false 失败
 */
bool gripper_save_config(uint8_t servo_id);

/**
 * @brief 加载夹爪配置从非易失性存储
 * @param servo_id 舵机ID
 * @return true 成功，false 失败
 */
bool gripper_load_config(uint8_t servo_id);

/**
 * @brief 执行夹爪精度测试
 * @param servo_id 舵机ID
 * @param start_percent 起始百分比
 * @param end_percent 结束百分比
 * @param step_percent 步长百分比
 * @return true 成功，false 失败
 */
bool gripper_precision_test(uint8_t servo_id, float start_percent, float end_percent, float step_percent);

/**
 * @brief 夹爪摩擦参数自学习
 * @param servo_id 舵机ID
 * @return true 成功，false 失败
 */
bool gripper_learn_friction_params(uint8_t servo_id);

/**
 * @brief 获取夹爪控制系统运行状态
 * @return true 系统正常运行，false 系统停止或异常
 */
bool gripper_controller_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* GRIPPER_CONTROLLER_H */

/************************ COPYRIGHT(C) SENTRY TEAM **************************/
