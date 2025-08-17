#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_task.h"
#include "esp_log.h"
#include <string.h>
#include "serial_servo.h"  // 添加SerialServo库

// 包含 uart_parser 模块的头文件
extern "C" {
#include "uart_parser.h"
#include "data_service.h"
#include "my_wifi_task.h"
#include "my_data_publisher_task.h"
}

#define MAIN_TASK_TAG "MAIN"

// Example WiFi credentials
#define EXAMPLE_ESP_WIFI_SSID      "opti_track_xiaomi"
#define EXAMPLE_ESP_WIFI_PASS      "sysu_opti_track"

// 串口舵机配置
#define SERVO_UART_NUM     2         // 使用Serial2
#define SERVO_RX_PIN       16        // 舵机串口RX引脚
#define SERVO_TX_PIN       17        // 舵机串口TX引脚
#define SERVO_BAUD_RATE    115200    // 舵机串口波特率
#define SERVO_ID           1         // 默认舵机ID

// 创建串口舵机对象
SerialServo* servo_controller = nullptr;

// 为 uart_parser 模块实现串口发送函数
// uart_parser.c 中的 uart_parser_put_string 是弱函数，我们在这里提供强实现
extern "C" void uart_parser_put_string(const char *str)
{
    Serial.print(str);
}


// FreeRTOS 串口舵机任务
extern "C" void my_servo_task(void* parameter) {
    ESP_LOGI(MAIN_TASK_TAG, "Servo RTOS task started");
    
    static uint32_t servo_demo_step = 0;
    static uint32_t last_servo_time = 0;
    const uint32_t SERVO_DEMO_INTERVAL = 3000; // 3秒间隔
    
    while (1) {
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // 每3秒执行一次舵机演示动作
        if (current_time - last_servo_time >= SERVO_DEMO_INTERVAL) {
            if (servo_controller != nullptr) {
                float target_angle = 0;
                
                switch (servo_demo_step % 4) {
                    case 0:
                        target_angle = 100;     // 100度
                        ESP_LOGI(MAIN_TASK_TAG, "Moving servo to 100 degrees");
                        break;
                    case 1:
                        target_angle = 120;    // 120度
                        ESP_LOGI(MAIN_TASK_TAG, "Moving servo to 120 degrees");
                        break;
                    case 2:
                        target_angle = 140;   // 140度
                        ESP_LOGI(MAIN_TASK_TAG, "Moving servo to 140 degrees");
                        break;
                    case 3:
                        target_angle = 160;    // 回到160度
                        ESP_LOGI(MAIN_TASK_TAG, "Moving servo to 160 degrees");
                        break;
                }

                // 控制舵机移动，增加执行时间到4000ms
                t_FuncRet result = servo_controller->move_servo_immediate(SERVO_ID, target_angle, 4000);
                if (result == Operation_Success) {
                    ESP_LOGD(MAIN_TASK_TAG, "Servo command sent successfully (2000ms execution time)");
                } else {
                    ESP_LOGW(MAIN_TASK_TAG, "Failed to send servo command");
                }
                
                // 读取舵机状态信息
                float current_position = 0;
                if (servo_controller->read_servo_position(SERVO_ID, current_position) == Operation_Success) {
                    ESP_LOGD(MAIN_TASK_TAG, "Servo current position: %.1f degrees", current_position);
                }
                
                int temperature = 0;
                if (servo_controller->read_servo_temp(SERVO_ID, temperature) == Operation_Success) {
                    ESP_LOGD(MAIN_TASK_TAG, "Servo temperature: %d°C", temperature);
                }
                
                float voltage = 0;
                if (servo_controller->read_servo_voltage(SERVO_ID, voltage) == Operation_Success) {
                    ESP_LOGD(MAIN_TASK_TAG, "Servo voltage: %.2fV", voltage);
                }
            }
            
            servo_demo_step++;
            last_servo_time = current_time;
        }
        
        // 任务延时
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms间隔
    }
}

void setup() {
    Serial.begin(115200);
    // 稍作延时，等待串口监视器连接
    vTaskDelay(pdMS_TO_TICKS(1000)); 

    ESP_LOGI(MAIN_TASK_TAG, "ESP32 WiFi Task with Arduino");

    // 初始化DataPlatform数据服务层
    if (data_service_init() != pdPASS) {
        ESP_LOGE(MAIN_TASK_TAG, "Failed to initialize data service");
        return;
    }
    ESP_LOGI(MAIN_TASK_TAG, "DataPlatform initialized successfully");

    // 创建 uart_parser 任务
    if (xTaskCreate(uart_parser_task, "UART_Parser_Task", 4096, NULL, tskIDLE_PRIORITY + 2, NULL) != pdPASS) {
        ESP_LOGE(MAIN_TASK_TAG, "Failed to create UART Parser task");
    }

    // 初始化串口舵机
    ESP_LOGI(MAIN_TASK_TAG, "Initializing Serial Servo...");
    
    // 初始化Serial2用于舵机通信
    Serial2.begin(SERVO_BAUD_RATE, SERIAL_8N1, SERVO_RX_PIN, SERVO_TX_PIN);
    ESP_LOGI(MAIN_TASK_TAG, "Serial2 initialized: Baud=%d, RX=%d, TX=%d", 
             SERVO_BAUD_RATE, SERVO_RX_PIN, SERVO_TX_PIN);
    
    // 创建舵机控制对象
    servo_controller = new SerialServo(Serial2);
    if (servo_controller == nullptr) {
        ESP_LOGE(MAIN_TASK_TAG, "Failed to create servo controller");
    } else {
        // 初始化舵机串口
        if (servo_controller->begin(SERVO_BAUD_RATE) == Operation_Success) {
            ESP_LOGI(MAIN_TASK_TAG, "Servo controller initialized successfully");
            
            // ========== 舵机初始化测试和诊断 ==========
            ESP_LOGI(MAIN_TASK_TAG, "Starting servo initialization tests...");
            
            // 延时确保舵机完全启动
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // 1. 测试舵机连接和基本通信
            float test_position = 0;
            if (servo_controller->read_servo_position(SERVO_ID, test_position) == Operation_Success) {
                ESP_LOGI(MAIN_TASK_TAG, "✓ Servo ID %d connected, current position: %.1f degrees", 
                         SERVO_ID, test_position);
            } else {
                ESP_LOGE(MAIN_TASK_TAG, "✗ Cannot read servo position - communication failed!");
                return; // 通信失败则退出
            }
            
            // 2. 检查并重置舵机工作模式（关键步骤！）
            ESP_LOGI(MAIN_TASK_TAG, "Checking and resetting servo working mode...");
            
            // 读取当前工作模式
            int current_mode = 0;
            int current_speed = 0;
            if (servo_controller->get_servo_mode_and_speed(SERVO_ID, current_mode, current_speed) == Operation_Success) {
                ESP_LOGI(MAIN_TASK_TAG, "✓ Current servo mode: %s (%d), speed: %d", 
                         (current_mode == 0) ? "SERVO_MODE" : "MOTOR_MODE", current_mode, current_speed);
                
                // 如果是Motor模式，强制切换回Servo模式
                if (current_mode == 1) {
                    ESP_LOGW(MAIN_TASK_TAG, "⚠ Servo is in MOTOR mode, switching to SERVO mode...");
                    if (servo_controller->set_servo_mode_and_speed(SERVO_ID, 0, 0) == Operation_Success) {
                        ESP_LOGI(MAIN_TASK_TAG, "✓ Successfully switched to SERVO mode");
                        vTaskDelay(pdMS_TO_TICKS(300)); // 等待模式切换完成
                        
                        // 验证模式切换
                        if (servo_controller->get_servo_mode_and_speed(SERVO_ID, current_mode, current_speed) == Operation_Success) {
                            ESP_LOGI(MAIN_TASK_TAG, "✓ Verified mode: %s (%d)", 
                                     (current_mode == 0) ? "SERVO_MODE" : "MOTOR_MODE", current_mode);
                            if (current_mode != 0) {
                                ESP_LOGE(MAIN_TASK_TAG, "✗ Failed to switch to SERVO mode!");
                                return;
                            }
                        }
                    } else {
                        ESP_LOGE(MAIN_TASK_TAG, "✗ Failed to switch servo to SERVO mode");
                        return;
                    }
                }
            } else {
                ESP_LOGW(MAIN_TASK_TAG, "✗ Cannot read servo mode, assuming SERVO mode and continuing...");
            }
            
            // 3. 读取舵机状态信息
            int temperature = 0;
            if (servo_controller->read_servo_temp(SERVO_ID, temperature) == Operation_Success) {
                ESP_LOGI(MAIN_TASK_TAG, "✓ Servo temperature: %d°C", temperature);
            } else {
                ESP_LOGW(MAIN_TASK_TAG, "✗ Cannot read servo temperature");
            }
            
            float voltage = 0;
            if (servo_controller->read_servo_voltage(SERVO_ID, voltage) == Operation_Success) {
                ESP_LOGI(MAIN_TASK_TAG, "✓ Servo voltage: %.2fV", voltage);
            } else {
                ESP_LOGW(MAIN_TASK_TAG, "✗ Cannot read servo voltage");
            }
            
            // 4. 检查舵机电机负载状态
            bool load_status = false;
            if (servo_controller->get_servo_motor_load_status(SERVO_ID, load_status) == Operation_Success) {
                ESP_LOGI(MAIN_TASK_TAG, "✓ Servo motor load status: %s (%s)", 
                         load_status ? "LOADED" : "UNLOADED", load_status ? "true" : "false");
                
                // 如果舵机处于卸载状态，尝试加载
                if (!load_status) {
                    ESP_LOGW(MAIN_TASK_TAG, "⚠ Servo is in UNLOADED state, attempting to load motor...");
                    if (servo_controller->set_servo_motor_load(SERVO_ID, false) == Operation_Success) {
                        ESP_LOGI(MAIN_TASK_TAG, "✓ Servo motor loaded successfully");
                        vTaskDelay(pdMS_TO_TICKS(200)); // 等待加载完成
                        
                        // 再次检查状态
                        if (servo_controller->get_servo_motor_load_status(SERVO_ID, load_status) == Operation_Success) {
                            ESP_LOGI(MAIN_TASK_TAG, "✓ Verified motor load status: %s (%s)", 
                                     load_status ? "LOADED" : "UNLOADED", load_status ? "true" : "false");
                        }
                    } else {
                        ESP_LOGE(MAIN_TASK_TAG, "✗ Failed to load servo motor");
                    }
                }
            } else {
                ESP_LOGW(MAIN_TASK_TAG, "✗ Cannot read servo motor load status");
            }
            
            // 5. 读取舵机LED告警状态
            uint8_t led_alarm = 0;
            if (servo_controller->get_servo_led_alarm(SERVO_ID, led_alarm) == Operation_Success) {
                ESP_LOGI(MAIN_TASK_TAG, "✓ Servo LED alarm status: 0x%02X %s", 
                         led_alarm, (led_alarm == 0) ? "(No alarm)" : "(Alarm detected!)");
                if (led_alarm != 0) {
                    ESP_LOGW(MAIN_TASK_TAG, "⚠ Servo alarm detected - trying to clear alarm...");
                    
                    // 尝试清除告警（设置为无告警状态）
                    if (servo_controller->set_servo_led_alarm(SERVO_ID, 0) == Operation_Success) {
                        ESP_LOGI(MAIN_TASK_TAG, "✓ Alarm cleared successfully");
                        vTaskDelay(pdMS_TO_TICKS(200));
                        
                        // 再次检查告警状态
                        if (servo_controller->get_servo_led_alarm(SERVO_ID, led_alarm) == Operation_Success) {
                            ESP_LOGI(MAIN_TASK_TAG, "✓ Verified alarm status: 0x%02X %s", 
                                     led_alarm, (led_alarm == 0) ? "(No alarm)" : "(Still has alarm)");
                        }
                    } else {
                        ESP_LOGW(MAIN_TASK_TAG, "✗ Failed to clear servo alarm");
                    }
                }
            } else {
                ESP_LOGW(MAIN_TASK_TAG, "✗ Cannot read servo LED alarm status");
            }
            
            // 6. 执行小幅度测试移动（增加执行时间）
            ESP_LOGI(MAIN_TASK_TAG, "Performing movement test with extended time...");
            float initial_position = test_position;
            float test_target = initial_position + 10.0f; // 增加移动幅度到10度
            
            ESP_LOGI(MAIN_TASK_TAG, "Testing movement: %.1f° → %.1f°", initial_position, test_target);
            // 增加执行时间到3000ms（3秒）
            if (servo_controller->move_servo_immediate(SERVO_ID, test_target, 3000) == Operation_Success) {
                ESP_LOGI(MAIN_TASK_TAG, "✓ Test movement command sent (3000ms execution time)");
                
                // 增加等待时间到4秒，确保舵机有足够时间完成移动
                ESP_LOGI(MAIN_TASK_TAG, "Waiting 4 seconds for movement completion...");
                vTaskDelay(pdMS_TO_TICKS(4000));
                
                // 检查是否成功移动
                float final_position = 0;
                if (servo_controller->read_servo_position(SERVO_ID, final_position) == Operation_Success) {
                    float movement_diff = fabs(final_position - initial_position);
                    ESP_LOGI(MAIN_TASK_TAG, "Position after test move: %.1f° (moved %.1f°)", 
                             final_position, movement_diff);
                    
                    if (movement_diff > 2.0f) {
                        ESP_LOGI(MAIN_TASK_TAG, "✓ Servo movement test PASSED - servo is responsive");
                    } else {
                        ESP_LOGW(MAIN_TASK_TAG, "⚠ Servo movement test FAILED - position didn't change significantly");
                        ESP_LOGW(MAIN_TASK_TAG, "   Possible causes: alarm condition, mechanical obstruction, or power issues");
                        
                        // 尝试一个更大幅度的移动测试
                        ESP_LOGI(MAIN_TASK_TAG, "Trying larger movement test: %.1f° → %.1f°", 
                                 initial_position, initial_position + 20.0f);
                        if (servo_controller->move_servo_immediate(SERVO_ID, initial_position + 20.0f, 5000) == Operation_Success) {
                            vTaskDelay(pdMS_TO_TICKS(6000)); // 等待6秒
                            
                            float large_test_position = 0;
                            if (servo_controller->read_servo_position(SERVO_ID, large_test_position) == Operation_Success) {
                                float large_movement_diff = fabs(large_test_position - initial_position);
                                ESP_LOGI(MAIN_TASK_TAG, "Large movement result: %.1f° (moved %.1f°)", 
                                         large_test_position, large_movement_diff);
                            }
                        }
                    }
                } else {
                    ESP_LOGW(MAIN_TASK_TAG, "✗ Cannot verify movement - position read failed");
                }
                
                // 返回初始位置（增加执行时间）
                ESP_LOGI(MAIN_TASK_TAG, "Returning to initial position: %.1f°", initial_position);
                servo_controller->move_servo_immediate(SERVO_ID, initial_position, 3000);
                vTaskDelay(pdMS_TO_TICKS(4000)); // 等待4秒完成返回
            } else {
                ESP_LOGE(MAIN_TASK_TAG, "✗ Test movement command failed");
            }
            
            ESP_LOGI(MAIN_TASK_TAG, "========== Servo initialization tests completed ==========");
            
            // 创建舵机任务（测试完成后再创建任务）
            if (xTaskCreate(my_servo_task, "Servo_Task", 3072, NULL, tskIDLE_PRIORITY + 2, NULL) != pdPASS) {
                ESP_LOGE(MAIN_TASK_TAG, "Failed to create servo RTOS task");
            } else {
                ESP_LOGI(MAIN_TASK_TAG, "Servo RTOS task created successfully");
            }
            
        } else {
            ESP_LOGE(MAIN_TASK_TAG, "Failed to initialize servo controller");
        }
    }

    // Configure WiFi Task for STA mode with TCP client
    // wifi_task_config_t wifi_config;
    // memset(&wifi_config, 0, sizeof(wifi_task_config_t));

    // wifi_config.wifi_mode = WIFI_STA;
    // strncpy(wifi_config.ssid, EXAMPLE_ESP_WIFI_SSID, sizeof(wifi_config.ssid) - 1);
    // strncpy(wifi_config.password, EXAMPLE_ESP_WIFI_PASS, sizeof(wifi_config.password) - 1);
    
    // wifi_config.power_save = false; // Disable power saving for best performance
    // wifi_config.tx_power = WIFI_POWER_19_5dBm; // Set max power
    // wifi_config.sta_connect_timeout_ms = 15000; // 15 seconds timeout

    // // Configure network as TCP client
    // wifi_config.network_config.protocol = NETWORK_PROTOCOL_TCP_CLIENT;
    // strncpy(wifi_config.network_config.remote_host, "192.168.31.136", sizeof(wifi_config.network_config.remote_host) - 1); // 通常手机热点的网关IP
    // wifi_config.network_config.remote_port = 2233; // 手机端TCP服务器端口，您可以根据实际情况修改
    // wifi_config.network_config.auto_connect = true; // WiFi连接成功后自动开始TCP连接
    // wifi_config.network_config.connect_timeout_ms = 10000; // 10 seconds timeout for TCP connection

    // // 初始化 WiFi 配置
    // if (wifi_init_config(&wifi_config) != pdPASS) {
    //     ESP_LOGE(MAIN_TASK_TAG, "Failed to initialize WiFi config");
    // } else {
    //     // 创建 WiFi RTOS 任务
    //     if (xTaskCreate(my_wifi_task, "WiFi_Task", 4096, NULL, tskIDLE_PRIORITY + 2, NULL) != pdPASS) {
    //         ESP_LOGE(MAIN_TASK_TAG, "Failed to create WiFi RTOS task");
    //     } else {
    //         ESP_LOGI(MAIN_TASK_TAG, "WiFi RTOS task created successfully");
    //     }
    // }

    // // 创建数据发布任务
    // if (xTaskCreate(data_publisher_task, "Data_Publisher_Task", 4096, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
    //     ESP_LOGE(MAIN_TASK_TAG, "Failed to create data publisher task");
    // } else {
    //     ESP_LOGI(MAIN_TASK_TAG, "Data publisher task created successfully");
    // }
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
    
    // 主循环可以执行其他低优先级或非阻塞的任务
    vTaskDelay(pdMS_TO_TICKS(10));
}
