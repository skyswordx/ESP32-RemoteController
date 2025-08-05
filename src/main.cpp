#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_task.h"
#include "esp_log.h"
#include <string.h>

// 包含 uart_parser 模块的头文件
extern "C" {
#include "uart_parser.h"
#include "encoder_driver.h"
#include "joystick_driver.h"
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

// 编码器回调函数
extern "C" void encoder_position_changed(int32_t position, int32_t delta) {
    ESP_LOGI(MAIN_TASK_TAG, "编码器位置: %ld, 变化量: %ld", position, delta);
    
    // 可以通过网络发送编码器数据
    if (is_wifi_connected() && is_network_connected()) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "ENCODER:%ld,%ld\n", position, delta);
        network_send_string(buffer);
    }
}

extern "C" void encoder_button_changed(bool pressed) {
    ESP_LOGI(MAIN_TASK_TAG, "编码器按钮: %s", pressed ? "按下" : "释放");
    
    if (is_wifi_connected() && is_network_connected()) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "ENC_BTN:%s\n", pressed ? "1" : "0");
        network_send_string(buffer);
    }
}

// 摇杆数据回调函数
extern "C" void joystick_data_changed(const joystick_data_t* data) {
    if (!data->in_deadzone) {
        ESP_LOGI(MAIN_TASK_TAG, "摇杆位置: X=%d, Y=%d, 幅度=%.2f, 角度=%.1f°", 
                 data->x, data->y, data->magnitude, data->angle);
        
        // 通过网络发送摇杆数据
        if (is_wifi_connected() && is_network_connected()) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "JOYSTICK:%d,%d,%.2f,%.1f\n", 
                     data->x, data->y, data->magnitude, data->angle);
            network_send_string(buffer);
        }
    }
}

extern "C" void joystick_button_changed(bool pressed) {
    ESP_LOGI(MAIN_TASK_TAG, "摇杆按钮: %s", pressed ? "按下" : "释放");
    
    if (is_wifi_connected() && is_network_connected()) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "JOY_BTN:%s\n", pressed ? "1" : "0");
        network_send_string(buffer);
    }
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

    // Configure WiFi Task for STA mode with TCP client
    wifi_task_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_task_config_t));

    wifi_config.wifi_mode = WIFI_STA;
    strncpy(wifi_config.ssid, EXAMPLE_ESP_WIFI_SSID, sizeof(wifi_config.ssid) - 1);
    strncpy(wifi_config.password, EXAMPLE_ESP_WIFI_PASS, sizeof(wifi_config.password) - 1);
    
    wifi_config.power_save = false; // Disable power saving for best performance
    wifi_config.tx_power = WIFI_POWER_19_5dBm; // Set max power
    wifi_config.sta_connect_timeout_ms = 15000; // 15 seconds timeout

    // Configure network as TCP client
    wifi_config.network_config.protocol = NETWORK_PROTOCOL_TCP_CLIENT;
    strncpy(wifi_config.network_config.remote_host, "172.26.18.126", sizeof(wifi_config.network_config.remote_host) - 1); // 通常手机热点的网关IP
    wifi_config.network_config.remote_port = 2233; // 手机端TCP服务器端口，您可以根据实际情况修改
    wifi_config.network_config.auto_connect = true; // WiFi连接成功后自动开始TCP连接
    wifi_config.network_config.connect_timeout_ms = 10000; // 10 seconds timeout for TCP connection

    // Start WiFi Task
    if (wifi_task_start(&wifi_config) != pdPASS) {
        ESP_LOGE(MAIN_TASK_TAG, "Failed to start WiFi task");
    }

    // 初始化编码器
    encoder_config_t encoder_config = {
        .pin_a = 34,              // 编码器A相引脚
        .pin_b = 35,              // 编码器B相引脚
        .pin_button = 17,         // 编码器按钮引脚
        .use_pullup = true,      // 使用内部上拉电阻
        .steps_per_notch = 4     // 每个刻度4个步数（根据编码器型号调整）
    };
    
    if (encoder_init(&encoder_config) == ESP_OK) {
        ESP_LOGI(MAIN_TASK_TAG, "编码器初始化成功");
        encoder_set_callback(encoder_position_changed);
        encoder_set_button_callback(encoder_button_changed);
    } else {
        ESP_LOGE(MAIN_TASK_TAG, "编码器初始化失败");
    }

    // 初始化摇杆
    joystick_config_t joystick_config = {
        .pin_x = 33,             // X轴ADC引脚 (A0)
        .pin_y = 32,             // Y轴ADC引脚 (A3)
        .pin_button = 12,        // 摇杆按钮引脚
        .use_pullup = true,      // 按钮使用内部上拉
        .deadzone = 50,          // 死区大小
        .invert_x = false,       // X轴不反转
        .invert_y = true,        // Y轴反转（根据摇杆安装方向调整）
        .center_x = 0,           // 自动检测中心值
        .center_y = 0            // 自动检测中心值
    };
    
    if (joystick_init(&joystick_config) == ESP_OK) {
        ESP_LOGI(MAIN_TASK_TAG, "摇杆初始化成功");
        
        // 等待一下再校准中心位置
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(MAIN_TASK_TAG, "正在校准摇杆中心位置...");
        joystick_calibrate_center();
        
        joystick_set_callback(joystick_data_changed);
        joystick_set_button_callback(joystick_button_changed);
    } else {
        ESP_LOGE(MAIN_TASK_TAG, "摇杆初始化失败");
    }
}

void loop() { /* 类似 defaultTask */
    static bool message_sent = false;
    
    // 检查是否需要发送初始消息
    if (!message_sent && is_wifi_connected() && is_network_connected()) {
        ESP_LOGI(MAIN_TASK_TAG, "Sending hello message to TCP server...");
        int result = network_send_string("hello misakaa from esp32\n");
        if (result > 0) {
            ESP_LOGI(MAIN_TASK_TAG, "Message sent successfully (%d bytes)", result);
            message_sent = true;
        } else {
            ESP_LOGE(MAIN_TASK_TAG, "Failed to send message");
        }
    }
    
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
    
    // 处理编码器和摇杆任务
    encoder_task();
    joystick_task();
    
    // 主循环可以执行其他低优先级或非阻塞的任务
    vTaskDelay(pdMS_TO_TICKS(10));
}
