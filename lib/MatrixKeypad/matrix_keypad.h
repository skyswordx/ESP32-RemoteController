#ifndef MATRIX_KEYPAD_H
#define MATRIX_KEYPAD_H

#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "data_service.h"

#ifdef __cplusplus
extern "C" {
#endif

// 矩阵键盘配置结构体
typedef struct {
    uint8_t row_pins[3];      // 行引脚数组
    uint8_t col_pins[3];      // 列引脚数组
    bool use_pullup;          // 是否使用内部上拉电阻
    uint8_t debounce_time_ms; // 按键去抖时间(毫秒)
} keypad_config_t;

// 键盘按键数据结构
typedef struct {
    uint8_t key_code;         // 按键代码 (1-9)
    bool pressed;             // 是否按下
    uint32_t timestamp;       // 时间戳
} keypad_data_t;

// 按键事件回调函数类型
typedef void (*keypad_callback_t)(uint8_t key, bool pressed);

// 初始化矩阵键盘
esp_err_t keypad_init(const keypad_config_t* config);

// 获取按键状态
bool keypad_is_key_pressed(uint8_t key);

// 设置按键回调函数
void keypad_set_callback(keypad_callback_t callback);

// 键盘处理函数（需要在任务中调用）
void keypad_handler(void);

// 获取最后一次按下的按键
uint8_t keypad_get_last_key(void);

// 重置键盘状态
void keypad_reset(void);

#ifdef __cplusplus
}
#endif

#endif // MATRIX_KEYPAD_H
