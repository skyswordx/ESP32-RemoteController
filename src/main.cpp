#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_task.h"
#include "esp_log.h"
#include <string.h>

// 包含 uart_parser 模块的头文件
extern "C" {
#include "uart_parser.h"
}

#define MAIN_TASK_TAG "MAIN"

// Example WiFi credentials
#define EXAMPLE_ESP_WIFI_SSID      "misakaa"
#define EXAMPLE_ESP_WIFI_PASS      "Gg114514"

// 为 uart_parser 模块实现串口发送函数
// uart_parser.c 中的 uart_parser_put_string 是弱函数，我们在这里提供强实现
extern "C" void uart_parser_put_string(const char *str)
{
    Serial.print(str);
}


void setup() {
    Serial.begin(115200);
    // 稍作延时，等待串口监视器连接
    vTaskDelay(pdMS_TO_TICKS(1000)); 

    ESP_LOGI(MAIN_TASK_TAG, "ESP32 WiFi Task with Arduino");

    // 创建 uart_parser 任务
    if (xTaskCreate(uart_parser_task, "UART_Parser_Task", 4096, NULL, tskIDLE_PRIORITY + 2, NULL) != pdPASS) {
        ESP_LOGE(MAIN_TASK_TAG, "Failed to create UART Parser task");
    }

    // Configure WiFi Task for STA mode
    wifi_task_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_task_config_t));

    wifi_config.wifi_mode = WIFI_STA;
    strncpy(wifi_config.ssid, EXAMPLE_ESP_WIFI_SSID, sizeof(wifi_config.ssid) - 1);
    strncpy(wifi_config.password, EXAMPLE_ESP_WIFI_PASS, sizeof(wifi_config.password) - 1);
    
    wifi_config.power_save = false; // Disable power saving for best performance
    wifi_config.tx_power = WIFI_POWER_19_5dBm; // Set max power
    wifi_config.sta_connect_timeout_ms = 15000; // 15 seconds timeout

    // Start WiFi Task
    if (wifi_task_start(&wifi_config) != pdPASS) {
        ESP_LOGE(MAIN_TASK_TAG, "Failed to start WiFi task");
    }
}

void loop() {
    // 检查串口是否有数据输入
    if (Serial.available() > 0) {
        static char rx_buffer[128]; // 增加缓冲区大小以适应更长的命令
        static uint8_t rx_counter = 0;
        char received_char = Serial.read();

        // 回显字符
        Serial.write(received_char);

        // 判断是否是命令结束符 (回车或换行)
        if (received_char == '\r' || received_char == '\n') {
            if (rx_counter > 0) {
                // 命令接收完毕
                rx_buffer[rx_counter] = '\0'; // 添加字符串结束符

                // 将接收到的命令字符串发送到解析任务的队列
                uart_parser_send_command_to_queue(rx_buffer);

                // 重置计数器，准备接收下一条命令
                rx_counter = 0;
            }
        } else if (received_char == '\b' || received_char == 127) { // 处理退格键
            if (rx_counter > 0) {
                rx_counter--;
                // 在终端上回显退格、空格、退格，以实现删除效果
                Serial.print("\b \b");
            }
        } else {
            // 将字符存入缓冲区
            if (rx_counter < sizeof(rx_buffer) - 1) {
                rx_buffer[rx_counter++] = received_char;
            }
        }
    }
    
    // 主循环可以执行其他低优先级或非阻塞的任务
    vTaskDelay(pdMS_TO_TICKS(10));
}
