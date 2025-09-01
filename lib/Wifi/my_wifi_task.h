#ifndef MY_WIFI_TASK_H
#define MY_WIFI_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_task.h" // 包含必要的类型定义和函数声明

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi RTOS任务
 * 
 * 这个任务负责:
 * 1. 初始化WiFi连接
 * 2. 持续监控WiFi连接状态
 * 3. 在连接断开时自动重连
 * 4. 如果配置了网络协议，会在WiFi重连后尝试恢复网络连接
 * 
 * @param parameter 任务参数（未使用）
 */
void my_wifi_task(void* parameter);

#ifdef __cplusplus
}
#endif

#endif // MY_WIFI_TASK_H
