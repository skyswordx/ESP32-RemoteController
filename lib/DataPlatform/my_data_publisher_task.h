#ifndef MY_DATA_PUBLISHER_TASK_H
#define MY_DATA_PUBLISHER_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 数据发布任务 - 监听DataPlatform事件并通过网络发送
 * @param parameter 任务参数（未使用）
 */
void data_publisher_task(void* parameter);

#ifdef __cplusplus
}
#endif

#endif // MY_DATA_PUBLISHER_TASK_H
