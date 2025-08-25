#include "servo_controller.h"
#include "serial_servo.h"
#include "Arduino.h"
#include "esp_log.h"
#include <string.h>

#define SERVO_CONTROLLER_TAG "SERVO_CTRL"

// 全局变量
static servo_config_t g_servo_config;
static SerialServo* g_servo_controller = nullptr;
static bool g_servo_initialized = false;
static bool g_servo_connected = false;

// 夹爪映射参数
typedef struct {
    float closed_angle;    // 闭合角度
    float open_angle;      // 张开角度
    float min_step;        // 最小步进角度
    bool is_configured;    // 是否已配置
} gripper_mapping_t;

static gripper_mapping_t g_gripper_mapping = {
    .closed_angle = 160.0f,  // 根据您的描述，现在160是闭合
    .open_angle = 90.0f,     // 90是张开
    .min_step = 15.0f,       // 最小15度步进克服死区
    .is_configured = true
};

// 内部函数声明
static bool servo_hardware_init(void);
static bool servo_run_diagnostics(void);

extern "C" {

bool servo_controller_init(const servo_config_t *config) {
    if (config == nullptr) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Invalid config pointer");
        return false;
    }
    
    // 复制配置
    memcpy(&g_servo_config, config, sizeof(servo_config_t));
    
    ESP_LOGI(SERVO_CONTROLLER_TAG, "Servo controller config:");
    ESP_LOGI(SERVO_CONTROLLER_TAG, "  UART: %d, RX: %d, TX: %d", 
             g_servo_config.uart_num, g_servo_config.rx_pin, g_servo_config.tx_pin);
    ESP_LOGI(SERVO_CONTROLLER_TAG, "  Baud: %d, Default ID: %d", 
             g_servo_config.baud_rate, g_servo_config.default_servo_id);
    
    // 初始化硬件
    if (!servo_hardware_init()) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Hardware initialization failed");
        return false;
    }
    
    // 运行诊断测试 (诊断失败不会阻止系统启动，只记录警告)
    if (!servo_run_diagnostics()) {
        ESP_LOGW(SERVO_CONTROLLER_TAG, "Diagnostics failed, but continuing with servo initialization");
    }
    
    ESP_LOGI(SERVO_CONTROLLER_TAG, "Servo controller initialized successfully");
    return true;
}

void servo_controller_deinit(void) {
    // 清理SerialServo对象
    if (g_servo_controller != nullptr) {
        delete g_servo_controller;
        g_servo_controller = nullptr;
        ESP_LOGI(SERVO_CONTROLLER_TAG, "Servo controller cleaned up");
    }
    
    // 重置状态标志
    g_servo_initialized = false;
    g_servo_connected = false;
}

bool servo_is_connected(void) {
    return g_servo_connected;
}

bool servo_get_status(uint8_t servo_id, servo_status_t *status) {
    if (status == nullptr || !g_servo_initialized || g_servo_controller == nullptr) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Invalid parameters or servo not initialized");
        return false;
    }
    
    memset(status, 0, sizeof(servo_status_t));
    status->servo_id = servo_id;
    status->is_connected = g_servo_connected;
    status->last_update_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    if (!g_servo_connected) {
        ESP_LOGW(SERVO_CONTROLLER_TAG, "Servo not connected, returning default status");
        return true;
    }
    
    // 读取当前位置
    if (g_servo_controller->read_servo_position(servo_id, status->current_position) != Operation_Success) {
        ESP_LOGW(SERVO_CONTROLLER_TAG, "Failed to read servo position");
    }
    
    // 读取温度
    if (g_servo_controller->read_servo_temp(servo_id, status->temperature) != Operation_Success) {
        ESP_LOGW(SERVO_CONTROLLER_TAG, "Failed to read servo temperature");
    }
    
    // 读取电压
    if (g_servo_controller->read_servo_voltage(servo_id, status->voltage) != Operation_Success) {
        ESP_LOGW(SERVO_CONTROLLER_TAG, "Failed to read servo voltage");
    }
    
    // 读取工作模式和负载状态
    int current_mode = 0;
    int current_speed = 0;
    if (g_servo_controller->get_servo_mode_and_speed(servo_id, current_mode, current_speed) == Operation_Success) {
        status->work_mode = (current_mode == 0) ? SERVO_MODE_SERVO : SERVO_MODE_MOTOR;
        status->current_speed = current_speed;
        ESP_LOGD(SERVO_CONTROLLER_TAG, "Read servo mode: %d, speed: %d", current_mode, current_speed);
    } else {
        ESP_LOGW(SERVO_CONTROLLER_TAG, "Failed to read servo mode, using default values");
        status->work_mode = SERVO_MODE_SERVO;
        status->current_speed = 0;
    }
    
    // 读取负载状态
    bool load_status = false;
    if (g_servo_controller->get_servo_motor_load_status(servo_id, load_status) == Operation_Success) {
        status->load_state = load_status ? SERVO_LOAD_LOAD : SERVO_LOAD_UNLOAD;
        ESP_LOGD(SERVO_CONTROLLER_TAG, "Read servo load status: %s", load_status ? "LOADED" : "UNLOADED");
    } else {
        ESP_LOGW(SERVO_CONTROLLER_TAG, "Failed to read servo load status, using default value");
        status->load_state = SERVO_LOAD_LOAD;
    }
    
    ESP_LOGI(SERVO_CONTROLLER_TAG, "Servo %d status: pos=%.1f°, temp=%d°C, volt=%.2fV", 
             servo_id, status->current_position, status->temperature, status->voltage);
    
    return true;
}

bool servo_set_load_state(uint8_t servo_id, servo_load_state_t load_state) {
    if (!g_servo_initialized || g_servo_controller == nullptr) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Servo not initialized");
        return false;
    }
    
    if (!g_servo_connected) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Servo not connected");
        return false;
    }
    
    bool success = false;
    
    try {
        if (load_state == SERVO_LOAD_LOAD) {
            success = (g_servo_controller->set_servo_motor_load(servo_id, true) == Operation_Success);
            ESP_LOGI(SERVO_CONTROLLER_TAG, "Setting servo %d to LOAD state", servo_id);
        } else {
            success = (g_servo_controller->set_servo_motor_load(servo_id, false) == Operation_Success);
            ESP_LOGI(SERVO_CONTROLLER_TAG, "Setting servo %d to UNLOAD state", servo_id);
        }
        
        if (success) {
            ESP_LOGI(SERVO_CONTROLLER_TAG, "Successfully changed load state for servo %d", servo_id);
            vTaskDelay(pdMS_TO_TICKS(200)); // 添加延时让舵机处理装载状态变更
        } else {
            ESP_LOGE(SERVO_CONTROLLER_TAG, "Failed to change load state for servo %d", servo_id);
        }
    } catch (...) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Exception occurred while setting load state");
        success = false;
    }
    
    return success;
}

bool servo_set_work_mode(uint8_t servo_id, servo_mode_t mode) {
    if (!g_servo_initialized || g_servo_controller == nullptr) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Servo not initialized");
        return false;
    }
    
    if (!g_servo_connected) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Servo not connected");
        return false;
    }
    
    bool success = false;
    
    try {
        if (mode == SERVO_MODE_SERVO) {
            success = (g_servo_controller->set_servo_mode_and_speed(servo_id, 0, 0) == Operation_Success);
            ESP_LOGI(SERVO_CONTROLLER_TAG, "Setting servo %d to SERVO mode", servo_id);
        } else {
            success = (g_servo_controller->set_servo_mode_and_speed(servo_id, 1, 0) == Operation_Success);
            ESP_LOGI(SERVO_CONTROLLER_TAG, "Setting servo %d to MOTOR mode", servo_id);
        }
        
        if (success) {
            ESP_LOGI(SERVO_CONTROLLER_TAG, "Successfully changed work mode for servo %d", servo_id);
        } else {
            ESP_LOGE(SERVO_CONTROLLER_TAG, "Failed to change work mode for servo %d", servo_id);
        }
    } catch (...) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Exception occurred while setting work mode");
        success = false;
    }
    
    return success;
}

bool servo_control_position(uint8_t servo_id, float angle, uint32_t time_ms) {
    if (!g_servo_initialized || g_servo_controller == nullptr) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Servo not initialized");
        return false;
    }
    
    if (!g_servo_connected) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Servo not connected");
        return false;
    }
    
    if (angle < 0 || angle > 240) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Invalid angle: %.1f (valid range: 0-240)", angle);
        return false;
    }
    
    if (time_ms < 20 || time_ms > 30000) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Invalid time: %lu ms (valid range: 20-30000)", time_ms);
        return false;
    }
    
    bool success = false;
    
    try {
        // 首先确保舵机处于舵机模式
        ESP_LOGI(SERVO_CONTROLLER_TAG, "Ensuring servo %d is in SERVO mode before position control", servo_id);
        if (g_servo_controller->set_servo_mode_and_speed(servo_id, 0, 0) != Operation_Success) {
            ESP_LOGW(SERVO_CONTROLLER_TAG, "Warning: Failed to set servo mode, continuing anyway...");
        } else {
            vTaskDelay(pdMS_TO_TICKS(100)); // 等待模式切换完成
        }
        
        // 确保舵机处于装载状态
        ESP_LOGI(SERVO_CONTROLLER_TAG, "Ensuring servo %d is in LOAD state before position control", servo_id);
        if (g_servo_controller->set_servo_motor_load(servo_id, true) != Operation_Success) {
            ESP_LOGW(SERVO_CONTROLLER_TAG, "Warning: Failed to set load state, continuing anyway...");
        } else {
            vTaskDelay(pdMS_TO_TICKS(100)); // 等待装载状态设置完成
        }
        
        // 在舵机模式下移动到指定位置
        success = (g_servo_controller->move_servo_immediate(servo_id, angle, time_ms) == Operation_Success);
        
        if (success) {
            ESP_LOGI(SERVO_CONTROLLER_TAG, "Servo %d moving to %.1f° in %lu ms", servo_id, angle, time_ms);
        } else {
            ESP_LOGE(SERVO_CONTROLLER_TAG, "Failed to move servo %d to %.1f°", servo_id, angle);
        }
    } catch (...) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Exception occurred while controlling position");
        success = false;
    }
    
    return success;
}

bool servo_control_speed(uint8_t servo_id, int16_t speed) {
    if (!g_servo_initialized || g_servo_controller == nullptr) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Servo not initialized");
        return false;
    }
    
    if (!g_servo_connected) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Servo not connected");
        return false;
    }
    
    if (speed < -1000 || speed > 1000) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Invalid speed: %d (valid range: -1000 to 1000)", speed);
        return false;
    }
    
    bool success = false;
    
    try {
        // 在电机模式下设置速度
        success = (g_servo_controller->set_servo_mode_and_speed(servo_id, 1, speed) == Operation_Success);
        
        if (success) {
            ESP_LOGI(SERVO_CONTROLLER_TAG, "Servo %d motor speed set to %d", servo_id, speed);
        } else {
            ESP_LOGE(SERVO_CONTROLLER_TAG, "Failed to set motor speed for servo %d", servo_id);
        }
    } catch (...) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Exception occurred while controlling speed");
        success = false;
    }
    
    return success;
}

bool servo_configure_gripper_mapping(uint8_t servo_id, float closed_angle, float open_angle, float min_step) {
    // 参数验证
    if (closed_angle < 0 || closed_angle > 240 || open_angle < 0 || open_angle > 240) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Invalid angle range: closed=%.1f, open=%.1f (valid: 0-240)", closed_angle, open_angle);
        return false;
    }
    
    if (min_step < 1.0f || min_step > 50.0f) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Invalid min_step: %.1f (valid: 1.0-50.0)", min_step);
        return false;
    }
    
    if (fabs(closed_angle - open_angle) < min_step) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Angle range too small: %.1f degrees (min_step: %.1f)", 
                 fabs(closed_angle - open_angle), min_step);
        return false;
    }
    
    // 更新映射参数
    g_gripper_mapping.closed_angle = closed_angle;
    g_gripper_mapping.open_angle = open_angle;
    g_gripper_mapping.min_step = min_step;
    g_gripper_mapping.is_configured = true;
    
    ESP_LOGI(SERVO_CONTROLLER_TAG, "Gripper mapping configured for servo %d:", servo_id);
    ESP_LOGI(SERVO_CONTROLLER_TAG, "  Closed: %.1f°, Open: %.1f°, MinStep: %.1f°", 
             closed_angle, open_angle, min_step);
    
    return true;
}

bool servo_control_gripper(uint8_t servo_id, float gripper_percent, uint32_t time_ms) {
    if (!g_servo_initialized || g_servo_controller == nullptr) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Servo not initialized");
        return false;
    }
    
    if (!g_servo_connected) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Servo not connected");
        return false;
    }
    
    if (!g_gripper_mapping.is_configured) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Gripper mapping not configured");
        return false;
    }
    
    // 参数验证
    if (gripper_percent < 0 || gripper_percent > 100) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Invalid gripper percent: %.1f (valid: 0-100)", gripper_percent);
        return false;
    }
    
    if (time_ms < 20 || time_ms > 30000) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Invalid time: %lu ms (valid: 20-30000)", time_ms);
        return false;
    }
    
    // 计算目标角度（线性插值）
    float angle_range = g_gripper_mapping.open_angle - g_gripper_mapping.closed_angle;
    float target_angle = g_gripper_mapping.closed_angle + (angle_range * gripper_percent / 100.0f);
    
    // 获取当前位置
    float current_angle = 0;
    bool has_current_pos = (g_servo_controller->read_servo_position(servo_id, current_angle) == Operation_Success);
    
    if (has_current_pos) {
        float angle_diff = fabs(target_angle - current_angle);
        
        // 如果角度差小于最小步进，使用最小步进
        if (angle_diff > 0.1f && angle_diff < g_gripper_mapping.min_step) {
            float direction = (target_angle > current_angle) ? 1.0f : -1.0f;
            target_angle = current_angle + (direction * g_gripper_mapping.min_step);
            
            ESP_LOGW(SERVO_CONTROLLER_TAG, "Angle diff %.1f° < min_step %.1f°, using step movement", 
                     angle_diff, g_gripper_mapping.min_step);
        }
    }
    
    // 确保目标角度在有效范围内
    if (target_angle < 0) target_angle = 0;
    if (target_angle > 240) target_angle = 240;
    
    bool success = false;
    
    try {
        // 首先确保舵机处于舵机模式和装载状态
        ESP_LOGI(SERVO_CONTROLLER_TAG, "Setting gripper %d to %.1f%% (%.1f°)", 
                 servo_id, gripper_percent, target_angle);
        
        if (g_servo_controller->set_servo_mode_and_speed(servo_id, 0, 0) != Operation_Success) {
            ESP_LOGW(SERVO_CONTROLLER_TAG, "Warning: Failed to set servo mode");
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        
        if (g_servo_controller->set_servo_motor_load(servo_id, true) != Operation_Success) {
            ESP_LOGW(SERVO_CONTROLLER_TAG, "Warning: Failed to set load state");
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        
        // 执行位置控制
        success = (g_servo_controller->move_servo_immediate(servo_id, target_angle, time_ms) == Operation_Success);
        
        if (success) {
            ESP_LOGI(SERVO_CONTROLLER_TAG, "Gripper movement command sent successfully");
        } else {
            ESP_LOGE(SERVO_CONTROLLER_TAG, "Failed to send gripper movement command");
        }
    } catch (...) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Exception occurred while controlling gripper");
        success = false;
    }
    
    return success;
}

} // extern "C"

// 内部实现函数

static bool servo_hardware_init(void) {
    ESP_LOGI(SERVO_CONTROLLER_TAG, "Initializing servo hardware...");
    
    // 初始化串口
    if (g_servo_config.uart_num == 2) {
        Serial2.begin(g_servo_config.baud_rate, SERIAL_8N1, 
                     g_servo_config.rx_pin, g_servo_config.tx_pin);
        ESP_LOGI(SERVO_CONTROLLER_TAG, "Serial2 initialized: Baud=%d, RX=%d, TX=%d", 
                 g_servo_config.baud_rate, g_servo_config.rx_pin, g_servo_config.tx_pin);
    } else {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Unsupported UART number: %d", g_servo_config.uart_num);
        return false;
    }
    
    // 创建舵机控制对象
    g_servo_controller = new SerialServo(Serial2);
    if (g_servo_controller == nullptr) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Failed to create servo controller");
        return false;
    }
    
    // 初始化舵机控制器
    if (g_servo_controller->begin(g_servo_config.baud_rate) != Operation_Success) {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "Failed to initialize servo controller");
        delete g_servo_controller;
        g_servo_controller = nullptr;
        return false;
    }
    
    ESP_LOGI(SERVO_CONTROLLER_TAG, "Servo controller initialized successfully");
    g_servo_initialized = true;
    return true;
}

static bool servo_run_diagnostics(void) {
    ESP_LOGI(SERVO_CONTROLLER_TAG, "========== Starting servo diagnostics ==========");
    
    // 延时确保舵机完全启动
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 1. 测试舵机连接和基本通信
    float test_position = 0;
    if (g_servo_controller->read_servo_position(g_servo_config.default_servo_id, test_position) == Operation_Success) {
        ESP_LOGI(SERVO_CONTROLLER_TAG, "✓ Servo ID %d connected, current position: %.1f degrees", 
                 g_servo_config.default_servo_id, test_position);
        g_servo_connected = true;
    } else {
        ESP_LOGE(SERVO_CONTROLLER_TAG, "✗ Cannot read servo position - communication failed!");
        return false;
    }
    
    // 2. 检查并重置舵机工作模式
    ESP_LOGI(SERVO_CONTROLLER_TAG, "Checking and resetting servo working mode...");
    int current_mode = 0;
    int current_speed = 0;
    if (g_servo_controller->get_servo_mode_and_speed(g_servo_config.default_servo_id, current_mode, current_speed) == Operation_Success) {
        ESP_LOGI(SERVO_CONTROLLER_TAG, "✓ Current servo mode: %s (%d), speed: %d", 
                 (current_mode == 0) ? "SERVO_MODE" : "MOTOR_MODE", current_mode, current_speed);
        
        // 如果是Motor模式，强制切换回Servo模式
        if (current_mode == 1) {
            ESP_LOGW(SERVO_CONTROLLER_TAG, "⚠ Servo is in MOTOR mode, switching to SERVO mode...");
            if (g_servo_controller->set_servo_mode_and_speed(g_servo_config.default_servo_id, 0, 0) == Operation_Success) {
                ESP_LOGI(SERVO_CONTROLLER_TAG, "✓ Successfully switched to SERVO mode");
                vTaskDelay(pdMS_TO_TICKS(300));
            } else {
                ESP_LOGE(SERVO_CONTROLLER_TAG, "✗ Failed to switch servo to SERVO mode");
                return false;
            }
        }
    } else {
        ESP_LOGW(SERVO_CONTROLLER_TAG, "✗ Cannot read servo mode, assuming SERVO mode and continuing...");
    }
    
    // 3. 读取舵机状态信息
    int temperature = 0;
    if (g_servo_controller->read_servo_temp(g_servo_config.default_servo_id, temperature) == Operation_Success) {
        ESP_LOGI(SERVO_CONTROLLER_TAG, "✓ Servo temperature: %d°C", temperature);
    } else {
        ESP_LOGW(SERVO_CONTROLLER_TAG, "✗ Cannot read servo temperature");
    }
    
    float voltage = 0;
    if (g_servo_controller->read_servo_voltage(g_servo_config.default_servo_id, voltage) == Operation_Success) {
        ESP_LOGI(SERVO_CONTROLLER_TAG, "✓ Servo voltage: %.2fV", voltage);
    } else {
        ESP_LOGW(SERVO_CONTROLLER_TAG, "✗ Cannot read servo voltage");
    }
    
    // 4. 检查舵机电机负载状态
    bool load_status = false;
    if (g_servo_controller->get_servo_motor_load_status(g_servo_config.default_servo_id, load_status) == Operation_Success) {
        ESP_LOGI(SERVO_CONTROLLER_TAG, "✓ Servo motor load status: %s", 
                 load_status ? "LOADED" : "UNLOADED");
        
        if (!load_status) {
            ESP_LOGW(SERVO_CONTROLLER_TAG, "⚠ Servo is in UNLOADED state, attempting to load motor...");
            if (g_servo_controller->set_servo_motor_load(g_servo_config.default_servo_id, true) == Operation_Success) {
                ESP_LOGI(SERVO_CONTROLLER_TAG, "✓ Servo motor loaded successfully");
                vTaskDelay(pdMS_TO_TICKS(200));
            } else {
                ESP_LOGE(SERVO_CONTROLLER_TAG, "✗ Failed to load servo motor");
            }
        }
    } else {
        ESP_LOGW(SERVO_CONTROLLER_TAG, "✗ Cannot read servo motor load status");
    }
    
    // 5. 执行实用角度范围移动测试 (100°→120°→140°→160°)
    ESP_LOGI(SERVO_CONTROLLER_TAG, "Performing practical angle range movement test...");
    float initial_position = test_position;
    float test_angles[] = {100.0f, 120.0f, 140.0f, 160.0f};
    int num_angles = sizeof(test_angles) / sizeof(test_angles[0]);
    
    for (int i = 0; i < num_angles; i++) {
        ESP_LOGI(SERVO_CONTROLLER_TAG, "Testing movement: %.1f° → %.1f°", 
                 (i == 0) ? initial_position : test_angles[i-1], test_angles[i]);
        
        if (g_servo_controller->move_servo_immediate(g_servo_config.default_servo_id, test_angles[i], 2000) == Operation_Success) {
            ESP_LOGI(SERVO_CONTROLLER_TAG, "✓ Movement command sent to %.1f°", test_angles[i]);
            vTaskDelay(pdMS_TO_TICKS(2500)); // 等待移动完成
            
            // 验证位置
            float final_position = 0;
            if (g_servo_controller->read_servo_position(g_servo_config.default_servo_id, final_position) == Operation_Success) {
                float position_error = fabs(final_position - test_angles[i]);
                ESP_LOGI(SERVO_CONTROLLER_TAG, "Position after move: %.1f° (target: %.1f°, error: %.1f°)", 
                         final_position, test_angles[i], position_error);
                
                if (position_error < 5.0f) {
                    ESP_LOGI(SERVO_CONTROLLER_TAG, "✓ Movement test PASSED for %.1f°", test_angles[i]);
                } else {
                    ESP_LOGW(SERVO_CONTROLLER_TAG, "⚠ Movement test WARNING for %.1f° - large error detected", test_angles[i]);
                }
            }
        } else {
            ESP_LOGE(SERVO_CONTROLLER_TAG, "✗ Movement command failed for %.1f°", test_angles[i]);
        }
    }
    
    // 返回初始位置
    ESP_LOGI(SERVO_CONTROLLER_TAG, "Returning to initial position: %.1f°", initial_position);
    g_servo_controller->move_servo_immediate(g_servo_config.default_servo_id, initial_position, 3000);
    vTaskDelay(pdMS_TO_TICKS(3500));
    
    ESP_LOGI(SERVO_CONTROLLER_TAG, "========== Servo diagnostics completed ==========");
    return true;
}
