#include "encoder_driver.h"
#include "esp_log.h"

static const char* TAG = "ENCODER";

// 全局变量
static ESP32Encoder esp32_encoder;
static encoder_config_t encoder_config;
static encoder_callback_t position_callback = nullptr;
static encoder_button_callback_t button_callback = nullptr;

static int32_t last_position = 0;
static bool last_button_state = false;
static unsigned long last_button_time = 0;
static const unsigned long DEBOUNCE_DELAY = 80; // 增加防抖延时至80ms
static bool button_initialized = false;  // 添加初始化标志

// 编码器初始化
esp_err_t encoder_init(const encoder_config_t* config) {
    if (!config) {
        ESP_LOGE(TAG, "Config is null");
        return ESP_ERR_INVALID_ARG;
    }

    // 保存配置
    encoder_config = *config;

    // 初始化 ESP32Encoder 库
    ESP32Encoder::useInternalWeakPullResistors = config->use_pullup ? UP : NONE;
    esp32_encoder.attachHalfQuad(config->pin_a, config->pin_b);
    esp32_encoder.setCount(0);

    // 初始化按钮引脚（如果配置了）
    if (config->pin_button != 255) {
        pinMode(config->pin_button, config->use_pullup ? INPUT_PULLUP : INPUT);
        
        // 初始化按钮状态 - 额外添加初始化步骤
        button_initialized = false;
        vTaskDelay(pdMS_TO_TICKS(10)); // 给引脚状态稳定一些时间
    }

    ESP_LOGI(TAG, "Encoder initialized: PIN_A=%d, PIN_B=%d, BUTTON=%d", 
             config->pin_a, config->pin_b, config->pin_button);
    
    return ESP_OK;
}

// 获取编码器位置
int32_t encoder_get_position(void) {
    // ESP32Encoder 计数需要根据 steps_per_notch 进行调整
    int32_t raw_count = esp32_encoder.getCount();
    return raw_count / encoder_config.steps_per_notch;
}

// 重置编码器位置
void encoder_reset_position(void) {
    esp32_encoder.setCount(0);
    last_position = 0;
}

// 设置编码器回调函数
void encoder_set_callback(encoder_callback_t callback) {
    position_callback = callback;
}

void encoder_set_button_callback(encoder_button_callback_t callback) {
    button_callback = callback;
}

// 编码器处理函数
void encoder_handler(void) {
    // ESP32Encoder 不需要像 RotaryEncoder 那样调用 tick()
    // 它使用中断自动处理编码器信号
    
    // 检查位置变化
    int32_t current_position = encoder_get_position();
    if (current_position != last_position) {
        int32_t delta = current_position - last_position;
        last_position = current_position;
        
        ESP_LOGD(TAG, "Position: %ld, Delta: %ld", current_position, delta);
        
        // 更新到DataPlatform
        encoder_data_t encoder_data = {
            .position = current_position,
            .delta = delta,
            .button_pressed = encoder_get_button_state(),
            .timestamp = xTaskGetTickCount()
        };
        data_service_update_encoder(&encoder_data);
        
        // 调用回调函数（保持兼容性）
        if (position_callback) {
            position_callback(current_position, delta);
        }
    }

    // 检查按钮状态（如果配置了按钮）
    if (encoder_config.pin_button != 255) {
        bool current_button_state = digitalRead(encoder_config.pin_button);
        if (encoder_config.use_pullup) {
            current_button_state = !current_button_state; // 上拉时逻辑反转
        }
        
        unsigned long current_time = millis();
        
        // 首次初始化按钮状态
        if (!button_initialized) {
            if (current_time > 1000) {  // 系统启动1秒后再初始化按钮
                last_button_state = current_button_state; // 不触发回调，只记录初始状态
                last_button_time = current_time;
                button_initialized = true;
                ESP_LOGI(TAG, "Button initialized, initial state: %s", 
                         current_button_state ? "PRESSED" : "RELEASED");
            }
            return;  // 初始化阶段不处理按钮事件
        }
        
        // 状态变化检测和防抖处理
        if (current_button_state != last_button_state && 
            (current_time - last_button_time) > DEBOUNCE_DELAY) {
            
            // 再次检查状态，确保不是瞬态干扰
            vTaskDelay(pdMS_TO_TICKS(5));  // 短暂延时
            bool verify_state = digitalRead(encoder_config.pin_button);
            if (encoder_config.use_pullup) {
                verify_state = !verify_state;
            }
            
            // 如果状态一致，才认为是真实的按钮事件
            if (verify_state == current_button_state) {
                last_button_state = current_button_state;
                last_button_time = current_time;
                
                ESP_LOGD(TAG, "Button state: %s", current_button_state ? "PRESSED" : "RELEASED");
                
                // 调用按钮回调函数
                if (button_callback) {
                    button_callback(current_button_state);
                }
                
                // 更新到DataPlatform
                encoder_data_t encoder_data = {
                    .position = encoder_get_position(),
                    .delta = 0,  // 按钮事件不涉及位置变化
                    .button_pressed = current_button_state,
                    .timestamp = xTaskGetTickCount()
                };
                data_service_update_encoder(&encoder_data);
            }
        }
    }
}

// 获取编码器按钮状态
bool encoder_get_button_state(void) {
    if (encoder_config.pin_button == 255 || !button_initialized) {
        return false;  // 如果按钮未配置或未初始化，返回false
    }
    
    // 读取按钮状态 - 使用多次采样提高可靠性
    int samples = 0;
    for (int i = 0; i < 3; i++) {
        bool state = digitalRead(encoder_config.pin_button);
        if (encoder_config.use_pullup) {
            state = !state; // 上拉时逻辑反转
        }
        
        if (state) samples++;
        if (i < 2) vTaskDelay(pdMS_TO_TICKS(1));  // 每次采样间隔1ms
    }
    
    // 多数投票法确定最终状态
    return (samples >= 2);  // 至少2个样本为高电平，认为按钮被按下
}
