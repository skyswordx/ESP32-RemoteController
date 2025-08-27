#ifndef SERVO_COMMANDS_H
#define SERVO_COMMANDS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 处理获取舵机状态命令
 * @param argc 参数个数
 * @param argv 参数数组
 */
void handle_servo_status(int argc, char *argv[]);

/**
 * @brief 处理设置舵机负载状态命令  
 * @param argc 参数个数
 * @param argv 参数数组
 */
void handle_servo_load(int argc, char *argv[]);

/**
 * @brief 处理设置舵机工作模式命令
 * @param argc 参数个数
 * @param argv 参数数组
 */
void handle_servo_mode(int argc, char *argv[]);

/**
 * @brief 处理舵机位置控制命令
 * @param argc 参数个数
 * @param argv 参数数组
 */
void handle_servo_position(int argc, char *argv[]);

/**
 * @brief 处理舵机速度控制命令
 * @param argc 参数个数
 * @param argv 参数数组
 */
void handle_servo_speed(int argc, char *argv[]);

/**
 * @brief 处理夹爪控制命令
 * @param argc 参数个数
 * @param argv 参数数组
 */
void handle_servo_gripper(int argc, char *argv[]);

/**
 * @brief 处理夹爪映射配置命令
 * @param argc 参数个数
 * @param argv 参数数组
 */
void handle_servo_gripper_config(int argc, char *argv[]);

/**
 * @brief 处理夹爪丝滑控制命令
 * @param argc 参数个数
 * @param argv 参数数组
 */
void handle_servo_gripper_smooth(int argc, char *argv[]);

/**
 * @brief 处理夹爪状态查询命令
 * @param argc 参数个数
 * @param argv 参数数组
 */
void handle_servo_gripper_status(int argc, char *argv[]);

/**
 * @brief 处理夹爪模式设置命令
 * @param argc 参数个数
 * @param argv 参数数组
 */
void handle_servo_gripper_mode(int argc, char *argv[]);

/**
 * @brief 处理夹爪参数配置命令
 * @param argc 参数个数
 * @param argv 参数数组
 */
void handle_servo_gripper_params(int argc, char *argv[]);

/**
 * @brief 处理夹爪停止命令
 * @param argc 参数个数
 * @param argv 参数数组
 */
void handle_servo_gripper_stop(int argc, char *argv[]);

/**
 * @brief 处理夹爪校准命令
 * @param argc 参数个数
 * @param argv 参数数组
 */
void handle_servo_gripper_calibrate(int argc, char *argv[]);

/**
 * @brief 处理夹爪测试命令
 * @param argc 参数个数
 * @param argv 参数数组
 */
void handle_servo_gripper_test(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif // SERVO_COMMANDS_H
