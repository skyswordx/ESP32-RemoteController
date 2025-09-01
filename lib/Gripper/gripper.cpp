/**
 * @file gripper.cpp
 * @brief 夹爪控制器C++类实现文件
 * @version 2.0
 * @date 2025-08-25
 * @author skyswordx
 * 
 * @copyright Copyright (c) 2025
 * 
 * @note 简洁C++实现，直接将C函数转换为类成员函数，保持完全相同的算法
 */

#include "gripper.h"
#include <cstring>

/* Private macros ------------------------------------------------------------*/


/* Constructor and destructor -----------------------------------------------*/
Gripper_c::Gripper_c() {

    active_servo_id = 1;
    uart_port_id = 2;
    servo = SerialServo(uart_port_id);
    servo.begin(115200);
    
    pid_position = PID_controller_c();
}

Gripper_c::~Gripper_c() {
    // 释放资源
}


/* Private member functions -------------------------------------------------*/


/************************ COPYRIGHT(C) SENTRY TEAM **************************/
