/**
 * @file pid_controller.hpp
 * @brief 增强型PID控制器C++类实现 - 基于RoboWalker Class_PID设计
 * @version 2.0
 * @date 2025-08-25
 * @author GitHub Copilot
 * 
 * @copyright Copyright (c) 2025 SENTRY Team
 * 
 * @note 简洁C++实现，直接将C结构体转换为C++类，C函数转换为成员函数
 *       保持与原C版本完全相同的算法和接口
 */

#ifndef GRIPPER_H
#define GRIPPER_H

/* Includes ------------------------------------------------------------------*/
#include "serial_servo.h"
#include "pid_controller.hpp"

/* Exported types ------------------------------------------------------------*/

class Gripper_c {
public:
    /* 夹爪的舵机接口 */
    uint8_t uart_port_id = 2;            ///< 夹爪舵机所连接的UART端口
    uint32_t active_servo_id = 1;        ///< 夹爪舵机ID
    SerialServo servo;

    float angle_input_max = 147.00; // 146.64
    float angle_input_min = 101.00;       ///< 夹爪舵机输入角度最小值

    PID_controller_c pid_position;

    static uint32_t total_servo_count;   ///< 当前舵机总数
    static uint32_t total_gripper_count; ///< 当前夹爪总数
    
    /* 构造与析构 */
    Gripper_c();
    ~Gripper_c();

private:
    
};

#endif /* GRIPPER_H */

/************************ COPYRIGHT(C) **************************/
