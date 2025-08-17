#include "my_data_publisher_task.h"
#include "data_service.h"
#include "esp_log.h"
#include <stdio.h>

#define DATA_PUBLISHER_TAG "MY_DATA_PUBLISHER"

// WiFi和网络状态检查函数的声明，实现在wifi_task.cpp中
extern bool is_wifi_connected(void);
extern bool is_network_connected(void);
extern int network_send_string(const char* str);

// 数据发布任务 - 监听DataPlatform事件并通过网络发送
void data_publisher_task(void* parameter) {
    ESP_LOGI(DATA_PUBLISHER_TAG, "Data publisher task started (no sensors to monitor)");
    
    // 由于移除了encoder和joystick，暂时没有数据需要发布
    // 任务保持运行但不执行任何操作
    while (1) {
        // 延时等待，避免占用过多CPU
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
