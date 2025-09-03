#include "matrix_keypad.h"
#include "uart_parser.h" // 用于串口输出
// 调试标签
static const char* TAG = "KEYPAD";

// 全局变量
static keypad_config_t keypad_config;
static keypad_callback_t key_callback = NULL;
static bool key_states[9] = {false}; // 按键状态数组 (1-9)
static uint32_t key_last_change[9] = {0}; // 按键最后一次变化时间
static uint8_t last_key_pressed = 0; // 最后一次按下的按键

// 按键映射表 - 将行列坐标映射到按键码
// 按键布局:
// 1 2 3
// 4 5 6
// 7 8 9
static const uint8_t KEY_MAP[3][3] = {
    {1, 2, 3},
    {4, 5, 6},
    {7, 8, 9}
};

// 初始化矩阵键盘
esp_err_t keypad_init(const keypad_config_t* config) {
    if (!config) {
        ESP_LOGE(TAG, "Config is null");
        return ESP_ERR_INVALID_ARG;
    }

    // 保存配置
    keypad_config = *config;

    // 初始化行引脚为输出
    for (int i = 0; i < 3; i++) {
        pinMode(config->row_pins[i], OUTPUT);
        digitalWrite(config->row_pins[i], HIGH); // 初始状态为高电平
    }

    // 初始化列引脚为输入
    for (int i = 0; i < 3; i++) {
        pinMode(config->col_pins[i], config->use_pullup ? INPUT_PULLUP : INPUT);
    }

    // 重置按键状态
    keypad_reset();

    ESP_LOGI(TAG, "Matrix keypad initialized: Rows[%d,%d,%d], Cols[%d,%d,%d], Pullup:%s", 
            config->row_pins[0], config->row_pins[1], config->row_pins[2],
            config->col_pins[0], config->col_pins[1], config->col_pins[2],
            config->use_pullup ? "Enabled" : "Disabled");
    
    return ESP_OK;
}

// 获取按键状态
bool keypad_is_key_pressed(uint8_t key) {
    if (key < 1 || key > 9) {
        return false;
    }
    return key_states[key - 1];
}

// 设置按键回调函数
void keypad_set_callback(keypad_callback_t callback) {
    key_callback = callback;
}

// 重置键盘状态
void keypad_reset(void) {
    for (int i = 0; i < 9; i++) {
        key_states[i] = false;
        key_last_change[i] = 0;
    }
    last_key_pressed = 0;
}

// 获取最后一次按下的按键
uint8_t keypad_get_last_key(void) {
    return last_key_pressed;
}

// 键盘扫描与处理
void keypad_handler(void) {
    uint32_t current_time = millis();
    
    // 扫描键盘矩阵
    for (int row = 0; row < 3; row++) {
        // 激活当前行（设置为低电平）
        digitalWrite(keypad_config.row_pins[row], LOW);
        
        // 短暂延时，确保电平稳定
        delayMicroseconds(10);
        
        // 读取所有列状态
        for (int col = 0; col < 3; col++) {
            uint8_t key_index = KEY_MAP[row][col] - 1; // 按键索引 (0-8)
            bool key_pressed;
            
            // 读取列引脚状态
            int pin_state = digitalRead(keypad_config.col_pins[col]);
            
            // 根据上拉/下拉配置确定按键状态
            if (keypad_config.use_pullup) {
                key_pressed = (pin_state == LOW); // 上拉时，低电平表示按下
            } else {
                key_pressed = (pin_state == HIGH); // 下拉时，高电平表示按下
            }
            
            // 检测按键状态变化，并进行去抖处理
            if (key_pressed != key_states[key_index]) {
                // 如果经过了去抖时间，更新按键状态
                if ((current_time - key_last_change[key_index]) >= keypad_config.debounce_time_ms) {
                    key_states[key_index] = key_pressed;
                    key_last_change[key_index] = current_time;
                    
                    uint8_t key = key_index + 1; // 实际按键值 (1-9)
                    
                    // 更新最后按下的按键
                    if (key_pressed) {
                        last_key_pressed = key;
                    }
                    
                    // 打印按键信息
                    ESP_LOGI(TAG, "Key %d %s", key, key_pressed ? "PRESSED" : "RELEASED");
                    
                    // 发送到串口
                    char buffer[50];
                    snprintf(buffer, sizeof(buffer), "矩阵键盘: 按键 %d %s\r\n", 
                             key, key_pressed ? "按下" : "释放");
                    uart_parser_put_string(buffer);
                    
                    // 调用回调函数
                    if (key_callback) {
                        key_callback(key, key_pressed);
                    }
                    
                    // 更新到DataPlatform (如果需要)
                    // 此处可以根据需要添加数据服务更新代码
                }
            }
        }
        
        // 恢复当前行（设置为高电平）
        digitalWrite(keypad_config.row_pins[row], HIGH);
    }
}
