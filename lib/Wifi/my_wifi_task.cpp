#include "my_wifi_task.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define WIFI_TASK_TAG "MY_WIFI_TASK"

// WiFi处理函数的声明，实现在wifi_task.cpp中
extern void wifi_handler(void);

// 连接状态标志位
volatile bool wifi_task_running = false;
volatile bool connection_retry_in_progress = false;

// FreeRTOS WiFi 任务
void my_wifi_task(void* parameter) {
    ESP_LOGI(WIFI_TASK_TAG, "WiFi RTOS task started");
    
    // 设置任务运行标志
    wifi_task_running = true;
    
    // 调用WiFi处理函数进行初始化
    wifi_handler();
    
    // 进入持续监控模式
    TickType_t last_check_time = xTaskGetTickCount();
    TickType_t check_interval = pdMS_TO_TICKS(5000); // 5秒检查一次
    
    while (wifi_task_running) {
        // 检查WiFi连接状态
        if (!is_wifi_connected()) {
            if (!connection_retry_in_progress) {
                ESP_LOGW(WIFI_TASK_TAG, "WiFi connection lost, attempting to reconnect");
                connection_retry_in_progress = true;
                
                // 获取当前WiFi配置
                wifi_task_config_t config;
                if (get_current_wifi_config(&config)) {
                    // 尝试重新连接
                    ESP_LOGI(WIFI_TASK_TAG, "Reconnecting to WiFi SSID: %s", config.ssid);
                    
                    // 断开当前连接并重新连接
                    wifi_disconnect();
                    vTaskDelay(pdMS_TO_TICKS(1000)); // 等待断开完成
                    
                    if (wifi_connect_new(config.ssid, config.password, config.sta_connect_timeout_ms)) {
                        ESP_LOGI(WIFI_TASK_TAG, "WiFi reconnected successfully");
                        
                        // 如果配置了网络协议且需要自动连接，尝试重启网络连接
                        if (config.network_config.protocol != NETWORK_PROTOCOL_NONE && 
                            config.network_config.auto_connect) {
                            
                            ESP_LOGI(WIFI_TASK_TAG, "Restarting network connection");
                            
                            // 先断开现有网络连接
                            network_disconnect();
                            
                            // 根据网络协议类型重新连接
                            switch (config.network_config.protocol) {
                                case NETWORK_PROTOCOL_TCP_CLIENT:
                                    if (network_connect_tcp_client(
                                            config.network_config.remote_host,
                                            config.network_config.remote_port,
                                            config.network_config.connect_timeout_ms)) {
                                        ESP_LOGI(WIFI_TASK_TAG, "TCP Client reconnected successfully");
                                    } else {
                                        ESP_LOGE(WIFI_TASK_TAG, "Failed to reconnect TCP Client");
                                    }
                                    break;
                                    
                                // 其他网络协议类型的重连可以在这里添加
                                default:
                                    break;
                            }
                        }
                    } else {
                        ESP_LOGE(WIFI_TASK_TAG, "WiFi reconnection failed");
                    }
                }
                connection_retry_in_progress = false;
            }
        }
        
        // 使用vTaskDelayUntil以确保固定间隔检查
        vTaskDelayUntil(&last_check_time, check_interval);
    }
    
    // 如果任务被请求停止，在这里释放资源
    ESP_LOGI(WIFI_TASK_TAG, "WiFi task stopping");
    vTaskDelete(NULL);
}
