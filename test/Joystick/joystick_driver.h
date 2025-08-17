#ifndef JOYSTICK_DRIVER_H
#define JOYSTICK_DRIVER_H

#include "Arduino.h"
#include "data_service.h"

#ifdef __cplusplus
extern "C" {
#endif

// 摇杆配置结构体
typedef struct {
    uint8_t pin_x;           // X轴模拟输入引脚
    uint8_t pin_y;           // Y轴模拟输入引脚
    uint8_t pin_button;      // 摇杆按钮引脚（可选）
    bool use_pullup;         // 按钮是否使用内部上拉电阻
    uint16_t deadzone;       // 死区大小 (0-512)
    bool invert_x;           // 是否反转X轴
    bool invert_y;           // 是否反转Y轴
    uint16_t center_x;       // X轴中心值校准
    uint16_t center_y;       // Y轴中心值校准
} joystick_config_t;

// 使用DataPlatform中定义的joystick_data_t结构体
// 摇杆回调函数类型
typedef void (*joystick_callback_t)(const joystick_data_t* data);
typedef void (*joystick_button_callback_t)(bool pressed);

// 摇杆初始化
esp_err_t joystick_init(const joystick_config_t* config);

// 读取摇杆数据
joystick_data_t joystick_read(void);

// 获取摇杆原始ADC值
void joystick_get_raw_values(uint16_t* x, uint16_t* y);

// 校准摇杆中心位置
esp_err_t joystick_calibrate_center(void);

// 设置回调函数
void joystick_set_callback(joystick_callback_t callback);
void joystick_set_button_callback(joystick_button_callback_t callback);

// 摇杆处理函数（需要在任务中调用）
void joystick_handler(void);

// 获取按钮状态
bool joystick_get_button_state(void);

// 设置死区大小
void joystick_set_deadzone(uint16_t deadzone);

// 打印摇杆状态（调试用）
void joystick_print_status(void);

#ifdef __cplusplus
}
#endif

#endif // JOYSTICK_DRIVER_H
