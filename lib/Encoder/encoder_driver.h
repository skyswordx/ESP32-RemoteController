#ifndef ENCODER_DRIVER_H
#define ENCODER_DRIVER_H

#include "Arduino.h"
#include "ESP32Encoder.h"
#include "data_service.h"

#ifdef __cplusplus
extern "C" {
#endif

// 编码器配置结构体
typedef struct {
    uint8_t pin_a;           // 编码器A相引脚
    uint8_t pin_b;           // 编码器B相引脚
    uint8_t pin_button;      // 编码器按钮引脚（可选）
    bool use_pullup;         // 是否使用内部上拉电阻
    int16_t steps_per_notch; // 每个刻度的步数
} encoder_config_t;

// 编码器回调函数类型
typedef void (*encoder_callback_t)(int32_t position, int32_t delta);
typedef void (*encoder_button_callback_t)(bool pressed);

// 编码器初始化
esp_err_t encoder_init(const encoder_config_t* config);

// 获取编码器位置
int32_t encoder_get_position(void);

// 重置编码器位置
void encoder_reset_position(void);

// 设置编码器回调函数
void encoder_set_callback(encoder_callback_t callback);
void encoder_set_button_callback(encoder_button_callback_t callback);

// 编码器任务处理（需要在主循环中调用）
void encoder_task(void);

// 获取编码器按钮状态
bool encoder_get_button_state(void);

#ifdef __cplusplus
}
#endif

#endif // ENCODER_DRIVER_H
