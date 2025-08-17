# XY摇杆模块驱动

这个模块提供了ESP32上双轴模拟摇杆的完整驱动功能，支持XY轴读取、按钮输入、死区处理和校准功能。

## 功能特性

- 双轴模拟输入（X轴、Y轴）
- 可选的按钮输入支持
- 自动死区处理
- 中心位置校准
- 实时计算摇杆幅度和角度
- 数据变化回调功能
- 防抖处理
- 轴反转支持

## 硬件连接

### 标准XY摇杆模块连接
```
摇杆 VRX (X轴) -> ESP32 ADC引脚 (例如 GPIO36/A0)
摇杆 VRY (Y轴) -> ESP32 ADC引脚 (例如 GPIO39/A3)
摇杆 SW (按钮) -> ESP32 GPIO (例如 GPIO5)
摇杆 VCC -> 3.3V 或 5V
摇杆 GND -> GND
```

### 推荐的ADC引脚
ESP32的ADC引脚推荐使用：
- ADC1: GPIO32, GPIO33, GPIO34, GPIO35, GPIO36, GPIO37, GPIO38, GPIO39
- ADC2: GPIO0, GPIO2, GPIO4, GPIO12, GPIO13, GPIO14, GPIO15, GPIO25, GPIO26, GPIO27

## 使用示例

### 基本初始化

```cpp
#include "joystick_driver.h"

// 摇杆数据回调函数
void joystick_data_changed(const joystick_data_t* data) {
    if (!data->in_deadzone) {
        Serial.printf("摇杆位置: X=%d, Y=%d, 幅度=%.2f, 角度=%.1f°\n", 
                     data->x, data->y, data->magnitude, data->angle);
    }
}

// 按钮回调函数
void joystick_button_changed(bool pressed) {
    Serial.printf("摇杆按钮: %s\n", pressed ? "按下" : "释放");
}

void setup() {
    Serial.begin(115200);
    
    // 配置摇杆
    joystick_config_t config = {
        .pin_x = 36,          // X轴ADC引脚
        .pin_y = 39,          // Y轴ADC引脚
        .pin_button = 5,      // 按钮引脚
        .use_pullup = true,   // 按钮使用内部上拉
        .deadzone = 50,       // 死区大小
        .invert_x = false,    // X轴不反转
        .invert_y = true,     // Y轴反转
        .center_x = 0,        // 自动检测中心值
        .center_y = 0         // 自动检测中心值
    };
    
    // 初始化摇杆
    if (joystick_init(&config) == ESP_OK) {
        Serial.println("摇杆初始化成功");
        
        // 校准中心位置
        Serial.println("请保持摇杆在中心位置，开始校准...");
        delay(2000);
        joystick_calibrate_center();
        
        // 设置回调函数
        joystick_set_callback(joystick_data_changed);
        joystick_set_button_callback(joystick_button_changed);
    }
}

void loop() {
    // 在主循环中调用摇杆任务
    joystick_task();
    
    delay(10);
}
```

### 高级用法

```cpp
// 直接读取摇杆数据
joystick_data_t data = joystick_read();
Serial.printf("X: %d, Y: %d, 按钮: %s\n", 
              data.x, data.y, data.button_pressed ? "按下" : "释放");

// 获取原始ADC值
uint16_t raw_x, raw_y;
joystick_get_raw_values(&raw_x, &raw_y);
Serial.printf("原始ADC值 - X: %d, Y: %d\n", raw_x, raw_y);

// 调整死区大小
joystick_set_deadzone(80);

// 打印详细状态信息
joystick_print_status();

// 重新校准
joystick_calibrate_center();
```

## API 参考

### 数据结构

#### joystick_config_t
摇杆配置结构体
- `pin_x`: X轴ADC引脚号
- `pin_y`: Y轴ADC引脚号
- `pin_button`: 按钮引脚号（255表示无按钮）
- `use_pullup`: 按钮是否使用内部上拉电阻
- `deadzone`: 死区大小（0-512）
- `invert_x`: 是否反转X轴
- `invert_y`: 是否反转Y轴
- `center_x`: X轴中心值校准（0为自动检测）
- `center_y`: Y轴中心值校准（0为自动检测）

#### joystick_data_t
摇杆数据结构体
- `x`, `y`: 映射后的轴值（-512到+512）
- `raw_x`, `raw_y`: 原始ADC值（0-4095）
- `button_pressed`: 按钮状态
- `in_deadzone`: 是否在死区内
- `magnitude`: 摇杆偏移量（0.0-1.0）
- `angle`: 摇杆角度（0-360度）

### 函数接口

#### 初始化函数
- `esp_err_t joystick_init(const joystick_config_t* config)` - 初始化摇杆

#### 数据读取函数
- `joystick_data_t joystick_read(void)` - 读取摇杆数据
- `void joystick_get_raw_values(uint16_t* x, uint16_t* y)` - 获取原始ADC值

#### 校准函数
- `esp_err_t joystick_calibrate_center(void)` - 校准中心位置

#### 回调函数设置
- `void joystick_set_callback(joystick_callback_t callback)` - 设置数据变化回调
- `void joystick_set_button_callback(joystick_button_callback_t callback)` - 设置按钮回调

#### 任务处理
- `void joystick_task(void)` - 摇杆任务处理（需要在主循环中调用）

#### 配置函数
- `void joystick_set_deadzone(uint16_t deadzone)` - 设置死区大小
- `bool joystick_get_button_state(void)` - 获取按钮状态
- `void joystick_print_status(void)` - 打印状态信息

## 坐标系统

摇杆使用标准笛卡尔坐标系：
- X轴：左负右正（-512到+512）
- Y轴：下负上正（-512到+512）
- 角度：从正X轴开始逆时针测量（0-360度）

## 死区处理

死区功能可以消除摇杆在中心位置的微小抖动：
- 当摇杆在死区内时，X和Y值都会被设置为0
- 死区大小可以通过 `deadzone` 参数配置
- 推荐死区大小：30-100（根据摇杆精度调整）

## 注意事项

1. 需要在主循环中定期调用 `joystick_task()` 以确保摇杆正常工作
2. 建议在首次使用时进行中心位置校准
3. ADC引脚选择要避免与WiFi冲突的引脚
4. 死区大小需要根据具体摇杆模块的精度进行调整
5. 数据回调只在数值有明显变化时触发，减少CPU占用

## 依赖库

- `adafruit/Adafruit Unified Sensor` - 传感器统一接口库

这些库已在 `platformio.ini` 中配置，PlatformIO会自动下载安装。
