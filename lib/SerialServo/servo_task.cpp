#include "servo_task.h"
#include "Arduino.h"
#include "serial_servo.h"
#include "esp_log.h"
#include <string.h>

#define SERVO_TASK_TAG "SERVO_TASK"

// 全局变量
static servo_task_config_t g_servo_config;
static SerialServo* g_servo_controller = nullptr;
static TaskHandle_t g_servo_task_handle = nullptr;
static bool g_servo_initialized = false;
static bool g_servo_connected = false;

// 内部函数声明
static void servo_task_function(void* parameter);
static bool servo_hardware_init(void);
static bool servo_run_diagnostics(void);

extern "C" {

BaseType_t servo_init_config(const servo_task_config_t *config) {
    if (config == nullptr) {
        ESP_LOGE(SERVO_TASK_TAG, "Invalid config pointer");
        return pdFAIL;
    }
    
    // 复制配置
    memcpy(&g_servo_config, config, sizeof(servo_task_config_t));
    
    ESP_LOGI(SERVO_TASK_TAG, "Servo config initialized:");
    ESP_LOGI(SERVO_TASK_TAG, "  UART: %d, RX: %d, TX: %d", 
             g_servo_config.uart_num, g_servo_config.rx_pin, g_servo_config.tx_pin);
    ESP_LOGI(SERVO_TASK_TAG, "  Baud: %d, ID: %d", 
             g_servo_config.baud_rate, g_servo_config.servo_id);
    ESP_LOGI(SERVO_TASK_TAG, "  Demo: %s, Interval: %lu ms", 
             g_servo_config.enable_demo ? "enabled" : "disabled", 
             g_servo_config.demo_interval);
    
    return pdPASS;
}

BaseType_t servo_start_task(void) {
    if (g_servo_task_handle != nullptr) {
        ESP_LOGW(SERVO_TASK_TAG, "Servo task already running");
        return pdPASS;
    }
    
    // 初始化硬件
    if (!servo_hardware_init()) {
        ESP_LOGE(SERVO_TASK_TAG, "Hardware initialization failed");
        return pdFAIL;
    }
    
    // 运行诊断测试
    if (!servo_run_diagnostics()) {
        ESP_LOGE(SERVO_TASK_TAG, "Diagnostics failed");
        return pdFAIL;
    }
    
    // 创建任务
    if (xTaskCreate(servo_task_function, "Servo_Task", 3072, NULL, 
                    tskIDLE_PRIORITY + 2, &g_servo_task_handle) != pdPASS) {
        ESP_LOGE(SERVO_TASK_TAG, "Failed to create servo task");
        return pdFAIL;
    }
    
    ESP_LOGI(SERVO_TASK_TAG, "Servo task created successfully");
    return pdPASS;
}

void servo_stop_task(void) {
    if (g_servo_task_handle != nullptr) {
        vTaskDelete(g_servo_task_handle);
        g_servo_task_handle = nullptr;
        ESP_LOGI(SERVO_TASK_TAG, "Servo task stopped");
    }
}

bool servo_is_connected(void) {
    return g_servo_connected;
}

bool servo_move_to_angle(float angle, uint32_t time_ms) {
    if (!g_servo_initialized || g_servo_controller == nullptr) {
        ESP_LOGE(SERVO_TASK_TAG, "Servo not initialized");
        return false;
    }
    
    t_FuncRet result = g_servo_controller->move_servo_immediate(g_servo_config.servo_id, angle, time_ms);
    if (result == Operation_Success) {
        ESP_LOGI(SERVO_TASK_TAG, "Moving servo to %.1f degrees (time: %lu ms)", angle, time_ms);
        return true;
    } else {
        ESP_LOGW(SERVO_TASK_TAG, "Failed to move servo to %.1f degrees", angle);
        return false;
    }
}

bool servo_read_position(float *position) {
    if (!g_servo_initialized || g_servo_controller == nullptr || position == nullptr) {
        return false;
    }
    
    return g_servo_controller->read_servo_position(g_servo_config.servo_id, *position) == Operation_Success;
}

bool servo_read_temperature(int *temperature) {
    if (!g_servo_initialized || g_servo_controller == nullptr || temperature == nullptr) {
        return false;
    }
    
    return g_servo_controller->read_servo_temp(g_servo_config.servo_id, *temperature) == Operation_Success;
}

bool servo_read_voltage(float *voltage) {
    if (!g_servo_initialized || g_servo_controller == nullptr || voltage == nullptr) {
        return false;
    }
    
    return g_servo_controller->read_servo_voltage(g_servo_config.servo_id, *voltage) == Operation_Success;
}

} // extern "C"

// 内部实现函数

static bool servo_hardware_init(void) {
    ESP_LOGI(SERVO_TASK_TAG, "Initializing servo hardware...");
    
    // 初始化串口
    if (g_servo_config.uart_num == 2) {
        Serial2.begin(g_servo_config.baud_rate, SERIAL_8N1, 
                     g_servo_config.rx_pin, g_servo_config.tx_pin);
        ESP_LOGI(SERVO_TASK_TAG, "Serial2 initialized: Baud=%d, RX=%d, TX=%d", 
                 g_servo_config.baud_rate, g_servo_config.rx_pin, g_servo_config.tx_pin);
    } else {
        ESP_LOGE(SERVO_TASK_TAG, "Unsupported UART number: %d", g_servo_config.uart_num);
        return false;
    }
    
    // 创建舵机控制对象
    g_servo_controller = new SerialServo(Serial2);
    if (g_servo_controller == nullptr) {
        ESP_LOGE(SERVO_TASK_TAG, "Failed to create servo controller");
        return false;
    }
    
    // 初始化舵机控制器
    if (g_servo_controller->begin(g_servo_config.baud_rate) != Operation_Success) {
        ESP_LOGE(SERVO_TASK_TAG, "Failed to initialize servo controller");
        delete g_servo_controller;
        g_servo_controller = nullptr;
        return false;
    }
    
    ESP_LOGI(SERVO_TASK_TAG, "Servo controller initialized successfully");
    g_servo_initialized = true;
    return true;
}

static bool servo_run_diagnostics(void) {
    ESP_LOGI(SERVO_TASK_TAG, "========== Starting servo diagnostics ==========");
    
    // 延时确保舵机完全启动
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 1. 测试舵机连接和基本通信
    float test_position = 0;
    if (g_servo_controller->read_servo_position(g_servo_config.servo_id, test_position) == Operation_Success) {
        ESP_LOGI(SERVO_TASK_TAG, "✓ Servo ID %d connected, current position: %.1f degrees", 
                 g_servo_config.servo_id, test_position);
        g_servo_connected = true;
    } else {
        ESP_LOGE(SERVO_TASK_TAG, "✗ Cannot read servo position - communication failed!");
        return false;
    }
    
    // 2. 检查并重置舵机工作模式
    ESP_LOGI(SERVO_TASK_TAG, "Checking and resetting servo working mode...");
    int current_mode = 0;
    int current_speed = 0;
    if (g_servo_controller->get_servo_mode_and_speed(g_servo_config.servo_id, current_mode, current_speed) == Operation_Success) {
        ESP_LOGI(SERVO_TASK_TAG, "✓ Current servo mode: %s (%d), speed: %d", 
                 (current_mode == 0) ? "SERVO_MODE" : "MOTOR_MODE", current_mode, current_speed);
        
        // 如果是Motor模式，强制切换回Servo模式
        if (current_mode == 1) {
            ESP_LOGW(SERVO_TASK_TAG, "⚠ Servo is in MOTOR mode, switching to SERVO mode...");
            if (g_servo_controller->set_servo_mode_and_speed(g_servo_config.servo_id, 0, 0) == Operation_Success) {
                ESP_LOGI(SERVO_TASK_TAG, "✓ Successfully switched to SERVO mode");
                vTaskDelay(pdMS_TO_TICKS(300));
                
                // 验证模式切换
                if (g_servo_controller->get_servo_mode_and_speed(g_servo_config.servo_id, current_mode, current_speed) == Operation_Success) {
                    ESP_LOGI(SERVO_TASK_TAG, "✓ Verified mode: %s (%d)", 
                             (current_mode == 0) ? "SERVO_MODE" : "MOTOR_MODE", current_mode);
                    if (current_mode != 0) {
                        ESP_LOGE(SERVO_TASK_TAG, "✗ Failed to switch to SERVO mode!");
                        return false;
                    }
                }
            } else {
                ESP_LOGE(SERVO_TASK_TAG, "✗ Failed to switch servo to SERVO mode");
                return false;
            }
        }
    } else {
        ESP_LOGW(SERVO_TASK_TAG, "✗ Cannot read servo mode, assuming SERVO mode and continuing...");
    }
    
    // 3. 读取舵机状态信息
    int temperature = 0;
    if (g_servo_controller->read_servo_temp(g_servo_config.servo_id, temperature) == Operation_Success) {
        ESP_LOGI(SERVO_TASK_TAG, "✓ Servo temperature: %d°C", temperature);
    } else {
        ESP_LOGW(SERVO_TASK_TAG, "✗ Cannot read servo temperature");
    }
    
    float voltage = 0;
    if (g_servo_controller->read_servo_voltage(g_servo_config.servo_id, voltage) == Operation_Success) {
        ESP_LOGI(SERVO_TASK_TAG, "✓ Servo voltage: %.2fV", voltage);
    } else {
        ESP_LOGW(SERVO_TASK_TAG, "✗ Cannot read servo voltage");
    }
    
    // 4. 检查舵机电机负载状态
    bool load_status = false;
    if (g_servo_controller->get_servo_motor_load_status(g_servo_config.servo_id, load_status) == Operation_Success) {
        ESP_LOGI(SERVO_TASK_TAG, "✓ Servo motor load status: %s", 
                 load_status ? "LOADED" : "UNLOADED");
        
        if (!load_status) {
            ESP_LOGW(SERVO_TASK_TAG, "⚠ Servo is in UNLOADED state, attempting to load motor...");
            if (g_servo_controller->set_servo_motor_load(g_servo_config.servo_id, false) == Operation_Success) {
                ESP_LOGI(SERVO_TASK_TAG, "✓ Servo motor loaded successfully");
                vTaskDelay(pdMS_TO_TICKS(200));
            } else {
                ESP_LOGE(SERVO_TASK_TAG, "✗ Failed to load servo motor");
            }
        }
    } else {
        ESP_LOGW(SERVO_TASK_TAG, "✗ Cannot read servo motor load status");
    }
    
    // 5. 读取舵机LED告警状态
    uint8_t led_alarm = 0;
    if (g_servo_controller->get_servo_led_alarm(g_servo_config.servo_id, led_alarm) == Operation_Success) {
        ESP_LOGI(SERVO_TASK_TAG, "✓ Servo LED alarm status: 0x%02X %s", 
                 led_alarm, (led_alarm == 0) ? "(No alarm)" : "(Alarm detected!)");
        if (led_alarm != 0) {
            ESP_LOGW(SERVO_TASK_TAG, "⚠ Servo alarm detected - trying to clear alarm...");
            if (g_servo_controller->set_servo_led_alarm(g_servo_config.servo_id, 0) == Operation_Success) {
                ESP_LOGI(SERVO_TASK_TAG, "✓ Alarm cleared successfully");
                vTaskDelay(pdMS_TO_TICKS(200));
            } else {
                ESP_LOGW(SERVO_TASK_TAG, "✗ Failed to clear servo alarm");
            }
        }
    } else {
        ESP_LOGW(SERVO_TASK_TAG, "✗ Cannot read servo LED alarm status");
    }
    
    // 6. 执行移动测试
    ESP_LOGI(SERVO_TASK_TAG, "Performing movement test...");
    float initial_position = test_position;
    float test_target = initial_position + 10.0f;
    
    ESP_LOGI(SERVO_TASK_TAG, "Testing movement: %.1f° → %.1f°", initial_position, test_target);
    if (g_servo_controller->move_servo_immediate(g_servo_config.servo_id, test_target, 3000) == Operation_Success) {
        ESP_LOGI(SERVO_TASK_TAG, "✓ Test movement command sent");
        vTaskDelay(pdMS_TO_TICKS(4000));
        
        float final_position = 0;
        if (g_servo_controller->read_servo_position(g_servo_config.servo_id, final_position) == Operation_Success) {
            float movement_diff = fabs(final_position - initial_position);
            ESP_LOGI(SERVO_TASK_TAG, "Position after test move: %.1f° (moved %.1f°)", 
                     final_position, movement_diff);
            
            if (movement_diff > 2.0f) {
                ESP_LOGI(SERVO_TASK_TAG, "✓ Servo movement test PASSED");
            } else {
                ESP_LOGW(SERVO_TASK_TAG, "⚠ Servo movement test FAILED - limited movement detected");
            }
        }
        
        // 返回初始位置
        ESP_LOGI(SERVO_TASK_TAG, "Returning to initial position: %.1f°", initial_position);
        g_servo_controller->move_servo_immediate(g_servo_config.servo_id, initial_position, 3000);
        vTaskDelay(pdMS_TO_TICKS(4000));
    } else {
        ESP_LOGE(SERVO_TASK_TAG, "✗ Test movement command failed");
    }
    
    ESP_LOGI(SERVO_TASK_TAG, "========== Servo diagnostics completed ==========");
    return true;
}

static void servo_task_function(void* parameter) {
    ESP_LOGI(SERVO_TASK_TAG, "Servo RTOS task started");
    
    if (!g_servo_config.enable_demo) {
        ESP_LOGI(SERVO_TASK_TAG, "Demo mode disabled, task will idle");
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        return;
    }
    
    static uint32_t servo_demo_step = 0;
    static uint32_t last_servo_time = 0;
    
    while (1) {
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // 执行演示动作
        if (current_time - last_servo_time >= g_servo_config.demo_interval) {
            if (g_servo_controller != nullptr && g_servo_connected) {
                float target_angle = 0;
                
                switch (servo_demo_step % 4) {
                    case 0:
                        target_angle = 100;
                        ESP_LOGI(SERVO_TASK_TAG, "Demo: Moving servo to 100 degrees");
                        break;
                    case 1:
                        target_angle = 120;
                        ESP_LOGI(SERVO_TASK_TAG, "Demo: Moving servo to 120 degrees");
                        break;
                    case 2:
                        target_angle = 140;
                        ESP_LOGI(SERVO_TASK_TAG, "Demo: Moving servo to 140 degrees");
                        break;
                    case 3:
                        target_angle = 160;
                        ESP_LOGI(SERVO_TASK_TAG, "Demo: Moving servo to 160 degrees");
                        break;
                }

                servo_move_to_angle(target_angle, 4000);
                
                // 读取状态信息
                float current_position = 0;
                if (servo_read_position(&current_position)) {
                    ESP_LOGD(SERVO_TASK_TAG, "Current position: %.1f degrees", current_position);
                }
                
                int temperature = 0;
                if (servo_read_temperature(&temperature)) {
                    ESP_LOGD(SERVO_TASK_TAG, "Temperature: %d°C", temperature);
                }
                
                float voltage = 0;
                if (servo_read_voltage(&voltage)) {
                    ESP_LOGD(SERVO_TASK_TAG, "Voltage: %.2fV", voltage);
                }
            }
            
            servo_demo_step++;
            last_servo_time = current_time;
        }
        
        // 任务延时
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
