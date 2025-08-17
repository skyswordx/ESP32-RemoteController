#ifndef MY_WIFI_TASK_H
#define MY_WIFI_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi RTOS任务
 * @param parameter 任务参数（未使用）
 */
void my_wifi_task(void* parameter);

#ifdef __cplusplus
}
#endif

#endif // MY_WIFI_TASK_H
