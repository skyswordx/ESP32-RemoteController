# 旋转编码器驱动模块

这个模块提供了ESP32上旋转编码器的完整驱动功能，支持增量编码器和按钮功能。

## 功能特性

- 支持标准增量编码器（A相、B相）
- 可选的按钮输入支持
- 自动防抖处理
- 位置变化回调功能
- 支持内部上拉电阻
- 兼容多种编码器库

## 硬件连接

### 基本编码器连接
```
编码器 A相 -> ESP32 GPIO (例如 GPIO2)
编码器 B相 -> ESP32 GPIO (例如 GPIO4)
编码器 VCC -> 3.3V
编码器 GND -> GND
```

### 带按钮的编码器连接
```
编码器 A相 -> ESP32 GPIO2
编码器 B相 -> ESP32 GPIO4
编码器按钮 -> ESP32 GPIO5
编码器 VCC -> 3.3V
编码器 GND -> GND
```

## 使用示例

### 基本初始化

```cpp
#include "encoder_driver.h"

// 编码器回调函数
void encoder_position_changed(int32_t position, int32_t delta) {
    Serial.printf("编码器位置: %ld, 变化量: %ld\n", position, delta);
}

// 按钮回调函数
void encoder_button_changed(bool pressed) {
    Serial.printf("编码器按钮: %s\n", pressed ? "按下" : "释放");
}

void setup() {
    Serial.begin(115200);
    
    // 配置编码器
    encoder_config_t config = {
        .pin_a = 2,
        .pin_b = 4,
        .pin_button = 5,
        .use_pullup = true,
        .steps_per_notch = 4
    };
    
    // 初始化编码器
    if (encoder_init(&config) == ESP_OK) {
        Serial.println("编码器初始化成功");
        
        // 设置回调函数
        encoder_set_callback(encoder_position_changed);
        encoder_set_button_callback(encoder_button_changed);
    }
}

void loop() {
    // 在主循环中调用编码器任务
    encoder_task();
    
    delay(10);
}
```

### 高级用法

```cpp
// 重置编码器位置
encoder_reset_position();

// 直接读取当前位置
int32_t current_position = encoder_get_position();

// 检查按钮状态
bool button_pressed = encoder_get_button_state();
```

## API 参考

### 数据结构

#### encoder_config_t
编码器配置结构体
- `pin_a`: A相引脚号
- `pin_b`: B相引脚号  
- `pin_button`: 按钮引脚号（255表示无按钮）
- `use_pullup`: 是否使用内部上拉电阻
- `steps_per_notch`: 每个物理刻度的电气步数

### 函数接口

#### 初始化函数
- `esp_err_t encoder_init(const encoder_config_t* config)` - 初始化编码器

#### 位置控制函数
- `int32_t encoder_get_position(void)` - 获取当前位置
- `void encoder_reset_position(void)` - 重置位置为0

#### 回调函数设置
- `void encoder_set_callback(encoder_callback_t callback)` - 设置位置变化回调
- `void encoder_set_button_callback(encoder_button_callback_t callback)` - 设置按钮回调

#### 任务处理
- `void encoder_task(void)` - 编码器任务处理（需要在主循环中调用）

#### 按钮控制
- `bool encoder_get_button_state(void)` - 获取按钮状态

## 注意事项

1. 需要在主循环中定期调用 `encoder_task()` 以确保编码器正常工作
2. 编码器的电气特性可能因型号而异，需要根据实际情况调整 `steps_per_notch` 参数
3. 建议使用内部上拉电阻以提高信号稳定性
4. 按钮防抖时间设置为50ms，可根据需要在源码中调整

## 依赖库

- `mathertel/RotaryEncoder` - 主要编码器处理库
- `madhephaestus/ESP32Encoder` - ESP32优化的编码器库

这些库已在 `platformio.ini` 中配置，PlatformIO会自动下载安装。
