#include "my_wifi_task.h"
#include "esp_log.h"

#define WIFI_TASK_TAG "MY_WIFI_TASK"

// WiFi处理函数的声明，实现在wifi_task.cpp中
extern void wifi_handler(void);

// FreeRTOS WiFi 任务
void my_wifi_task(void* parameter) {
    ESP_LOGI(WIFI_TASK_TAG, "WiFi RTOS task started");
    
    // 调用一次 WiFi 处理函数进行初始化
    wifi_handler();
    
    // WiFi 任务完成后删除自己
    ESP_LOGI(WIFI_TASK_TAG, "WiFi initialization completed, deleting task");
    vTaskDelete(NULL);
}
