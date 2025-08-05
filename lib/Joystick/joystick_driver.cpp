#include "joystick_driver.h"
#include "esp_log.h"
#include <math.h>

static const char* TAG = "JOYSTICK";

// 全局变量
static joystick_config_t joystick_config;
static joystick_callback_t data_callback = nullptr;
static joystick_button_callback_t button_callback = nullptr;

static bool last_button_state = false;
static unsigned long last_button_time = 0;
static const unsigned long DEBOUNCE_DELAY = 50; // 防抖延时 50ms
static const uint16_t ADC_MAX = 4095; // ESP32 ADC最大值

// 内部函数声明
static int16_t map_axis_value(uint16_t raw_value, uint16_t center, bool invert);
static float calculate_magnitude(int16_t x, int16_t y);
static float calculate_angle(int16_t x, int16_t y);

// 摇杆初始化
esp_err_t joystick_init(const joystick_config_t* config) {
    if (!config) {
        ESP_LOGE(TAG, "Config is null");
        return ESP_ERR_INVALID_ARG;
    }

    // 保存配置
    joystick_config = *config;

    // 设置默认中心值（如果未指定）
    if (joystick_config.center_x == 0) {
        joystick_config.center_x = ADC_MAX / 2;
    }
    if (joystick_config.center_y == 0) {
        joystick_config.center_y = ADC_MAX / 2;
    }

    // 初始化模拟输入引脚
    analogReadResolution(12); // 设置ADC分辨率为12位
    analogSetAttenuation(ADC_11db); // 设置衰减以支持3.3V输入

    // 初始化按钮引脚（如果配置了）
    if (config->pin_button != 255) {
        pinMode(config->pin_button, config->use_pullup ? INPUT_PULLUP : INPUT);
    }

    ESP_LOGI(TAG, "Joystick initialized: X_PIN=%d, Y_PIN=%d, BUTTON=%d, DEADZONE=%d", 
             config->pin_x, config->pin_y, config->pin_button, config->deadzone);
    
    return ESP_OK;
}

// 读取摇杆数据
joystick_data_t joystick_read(void) {
    joystick_data_t data;
    
    // 读取原始ADC值
    data.raw_x = analogRead(joystick_config.pin_x);
    data.raw_y = analogRead(joystick_config.pin_y);
    
    // 映射到 -512 到 +512 范围
    data.x = map_axis_value(data.raw_x, joystick_config.center_x, joystick_config.invert_x);
    data.y = map_axis_value(data.raw_y, joystick_config.center_y, joystick_config.invert_y);
    
    // 应用死区
    if (abs(data.x) < joystick_config.deadzone && abs(data.y) < joystick_config.deadzone) {
        data.x = 0;
        data.y = 0;
        data.in_deadzone = true;
    } else {
        data.in_deadzone = false;
    }
    
    // 计算幅度和角度
    data.magnitude = calculate_magnitude(data.x, data.y);
    data.angle = calculate_angle(data.x, data.y);
    
    // 读取按钮状态
    data.button_pressed = joystick_get_button_state();
    
    return data;
}

// 获取摇杆原始ADC值
void joystick_get_raw_values(uint16_t* x, uint16_t* y) {
    if (x) *x = analogRead(joystick_config.pin_x);
    if (y) *y = analogRead(joystick_config.pin_y);
}

// 校准摇杆中心位置
esp_err_t joystick_calibrate_center(void) {
    ESP_LOGI(TAG, "Starting joystick calibration...");
    
    // 采样多次以获得更准确的中心值
    const int samples = 100;
    uint32_t sum_x = 0, sum_y = 0;
    
    for (int i = 0; i < samples; i++) {
        sum_x += analogRead(joystick_config.pin_x);
        sum_y += analogRead(joystick_config.pin_y);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    joystick_config.center_x = sum_x / samples;
    joystick_config.center_y = sum_y / samples;
    
    ESP_LOGI(TAG, "Calibration complete: center_x=%d, center_y=%d", 
             joystick_config.center_x, joystick_config.center_y);
    
    return ESP_OK;
}

// 设置回调函数
void joystick_set_callback(joystick_callback_t callback) {
    data_callback = callback;
}

void joystick_set_button_callback(joystick_button_callback_t callback) {
    button_callback = callback;
}

// 摇杆任务处理
void joystick_task(void) {
    static joystick_data_t last_data = {0};
    
    // 读取当前数据
    joystick_data_t current_data = joystick_read();
    
    // 检查数据是否有变化（减少不必要的回调）
    bool data_changed = (abs(current_data.x - last_data.x) > 5 || 
                        abs(current_data.y - last_data.y) > 5 ||
                        current_data.in_deadzone != last_data.in_deadzone);
    
    if (data_changed && data_callback) {
        data_callback(&current_data);
    }
    
    // 检查按钮状态变化
    if (joystick_config.pin_button != 255) {
        bool current_button_state = joystick_get_button_state();
        unsigned long current_time = millis();
        
        if (current_button_state != last_button_state && 
            (current_time - last_button_time) > DEBOUNCE_DELAY) {
            
            last_button_state = current_button_state;
            last_button_time = current_time;
            
            if (button_callback) {
                button_callback(current_button_state);
            }
        }
    }
    
    last_data = current_data;
}

// 获取按钮状态
bool joystick_get_button_state(void) {
    if (joystick_config.pin_button == 255) {
        return false;
    }
    
    bool state = digitalRead(joystick_config.pin_button);
    if (joystick_config.use_pullup) {
        state = !state; // 上拉时逻辑反转
    }
    return state;
}

// 设置死区大小
void joystick_set_deadzone(uint16_t deadzone) {
    joystick_config.deadzone = deadzone;
    ESP_LOGI(TAG, "Deadzone set to: %d", deadzone);
}

// 打印摇杆状态
void joystick_print_status(void) {
    joystick_data_t data = joystick_read();
    
    ESP_LOGI(TAG, "Joystick Status:");
    ESP_LOGI(TAG, "  Raw: X=%d, Y=%d", data.raw_x, data.raw_y);
    ESP_LOGI(TAG, "  Mapped: X=%d, Y=%d", data.x, data.y);
    ESP_LOGI(TAG, "  Magnitude: %.2f, Angle: %.1f°", data.magnitude, data.angle);
    ESP_LOGI(TAG, "  In deadzone: %s", data.in_deadzone ? "YES" : "NO");
    ESP_LOGI(TAG, "  Button: %s", data.button_pressed ? "PRESSED" : "RELEASED");
}

// 内部函数实现

// 映射轴值到 -512 到 +512 范围
static int16_t map_axis_value(uint16_t raw_value, uint16_t center, bool invert) {
    int16_t mapped;
    
    if (raw_value >= center) {
        // 正方向 (center 到 ADC_MAX)
        mapped = map(raw_value, center, ADC_MAX, 0, 512);
    } else {
        // 负方向 (0 到 center)
        mapped = map(raw_value, 0, center, -512, 0);
    }
    
    // 限制在范围内
    mapped = constrain(mapped, -512, 512);
    
    // 应用反转
    if (invert) {
        mapped = -mapped;
    }
    
    return mapped;
}

// 计算摇杆偏移量（0.0 到 1.0）
static float calculate_magnitude(int16_t x, int16_t y) {
    float magnitude = sqrt(x * x + y * y) / 512.0f;
    return constrain(magnitude, 0.0f, 1.0f);
}

// 计算摇杆角度（0 到 360 度）
static float calculate_angle(int16_t x, int16_t y) {
    if (x == 0 && y == 0) {
        return 0.0f;
    }
    
    float angle = atan2(y, x) * 180.0f / PI;
    if (angle < 0) {
        angle += 360.0f;
    }
    
    return angle;
}
