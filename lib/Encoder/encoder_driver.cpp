#include "encoder_driver.h"
#include "esp_log.h"

static const char* TAG = "ENCODER";

// 全局变量
static RotaryEncoder* rotary_encoder = nullptr;
static ESP32Encoder esp32_encoder;
static encoder_config_t encoder_config;
static encoder_callback_t position_callback = nullptr;
static encoder_button_callback_t button_callback = nullptr;

static int32_t last_position = 0;
static bool last_button_state = false;
static unsigned long last_button_time = 0;
static const unsigned long DEBOUNCE_DELAY = 50; // 防抖延时 50ms

// 编码器初始化
esp_err_t encoder_init(const encoder_config_t* config) {
    if (!config) {
        ESP_LOGE(TAG, "Config is null");
        return ESP_ERR_INVALID_ARG;
    }

    // 保存配置
    encoder_config = *config;

    // 初始化 RotaryEncoder 库
    rotary_encoder = new RotaryEncoder(config->pin_a, config->pin_b, RotaryEncoder::LatchMode::TWO03);
    
    // 初始化 ESP32Encoder 库作为备选
    ESP32Encoder::useInternalWeakPullResistors = config->use_pullup ? UP : NONE;
    esp32_encoder.attachHalfQuad(config->pin_a, config->pin_b);
    esp32_encoder.setCount(0);

    // 初始化按钮引脚（如果配置了）
    if (config->pin_button != 255) {
        pinMode(config->pin_button, config->use_pullup ? INPUT_PULLUP : INPUT);
    }

    ESP_LOGI(TAG, "Encoder initialized: PIN_A=%d, PIN_B=%d, BUTTON=%d", 
             config->pin_a, config->pin_b, config->pin_button);
    
    return ESP_OK;
}

// 获取编码器位置
int32_t encoder_get_position(void) {
    if (rotary_encoder) {
        return rotary_encoder->getPosition();
    }
    return esp32_encoder.getCount() / 4; // ESP32Encoder 通常有4倍分辨率
}

// 重置编码器位置
void encoder_reset_position(void) {
    if (rotary_encoder) {
        rotary_encoder->setPosition(0);
    }
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

// 编码器任务处理
void encoder_task(void) {
    // 更新编码器状态
    if (rotary_encoder) {
        rotary_encoder->tick();
    }

    // 检查位置变化
    int32_t current_position = encoder_get_position();
    if (current_position != last_position) {
        int32_t delta = current_position - last_position;
        last_position = current_position;
        
        ESP_LOGD(TAG, "Position: %ld, Delta: %ld", current_position, delta);
        
        // 调用回调函数
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
        if (current_button_state != last_button_state && 
            (current_time - last_button_time) > DEBOUNCE_DELAY) {
            
            last_button_state = current_button_state;
            last_button_time = current_time;
            
            ESP_LOGD(TAG, "Button state: %s", current_button_state ? "PRESSED" : "RELEASED");
            
            // 调用按钮回调函数
            if (button_callback) {
                button_callback(current_button_state);
            }
        }
    }
}

// 获取编码器按钮状态
bool encoder_get_button_state(void) {
    if (encoder_config.pin_button == 255) {
        return false;
    }
    
    bool state = digitalRead(encoder_config.pin_button);
    if (encoder_config.use_pullup) {
        state = !state; // 上拉时逻辑反转
    }
    return state;
}
