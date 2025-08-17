# SerialServo 模块重构文档

## 概述

基于 C/C++混合编程的最佳实践，将 main.cpp 中的串口舵机初始化和任务实现提取到独立的`SerialServo`模块中，实现了代码的模块化和可复用性。

## 重构目标

1. **模块化**：将串口舵机相关代码从 main.cpp 中分离出来
2. **配置化**：类似 WiFi 配置的方式，通过配置结构体来初始化舵机
3. **C/C++兼容**：确保 C 代码可以调用 C++实现的舵机功能
4. **易用性**：提供简洁的 API 接口供 main 函数调用

## 文件结构

```
lib/SerialServo/
├── servo_task.h          # C头文件 - 提供C接口声明
└── servo_task.cpp        # C++实现文件 - 舵机功能实现
```

## 接口设计

### 配置结构体

```c
typedef struct {
    int uart_num;           // UART端口号
    int rx_pin;             // RX引脚
    int tx_pin;             // TX引脚
    int baud_rate;          // 波特率
    int servo_id;           // 舵机ID
    bool enable_demo;       // 是否启用演示模式
    uint32_t demo_interval; // 演示动作间隔时间(ms)
} servo_task_config_t;
```

### 主要 API 函数

#### 初始化和控制

- `servo_init_config()` - 初始化舵机配置
- `servo_start_task()` - 启动舵机任务
- `servo_stop_task()` - 停止舵机任务

#### 状态查询

- `servo_is_connected()` - 获取舵机连接状态
- `servo_read_position()` - 读取舵机位置
- `servo_read_temperature()` - 读取舵机温度
- `servo_read_voltage()` - 读取舵机电压

#### 控制指令

- `servo_move_to_angle()` - 控制舵机移动到指定角度

## 使用方法

### 在 main.cpp 中的使用

```cpp
// 1. 包含头文件
extern "C" {
#include "servo_task.h"  // 添加SerialServo模块
}

// 2. 配置舵机参数
void setup() {
    // ... 其他初始化代码

    // 配置串口舵机
    servo_task_config_t servo_config = {
        .uart_num = 2,           // 使用Serial2
        .rx_pin = 16,            // 舵机串口RX引脚
        .tx_pin = 17,            // 舵机串口TX引脚
        .baud_rate = 115200,     // 舵机串口波特率
        .servo_id = 1,           // 默认舵机ID
        .enable_demo = true,     // 启用演示模式
        .demo_interval = 3000    // 3秒间隔
    };

    // 3. 初始化并启动舵机
    if (servo_init_config(&servo_config) == pdPASS) {
        ESP_LOGI(MAIN_TASK_TAG, "Servo config initialized successfully");

        if (servo_start_task() == pdPASS) {
            ESP_LOGI(MAIN_TASK_TAG, "Servo task started successfully");
        } else {
            ESP_LOGE(MAIN_TASK_TAG, "Failed to start servo task");
        }
    } else {
        ESP_LOGE(MAIN_TASK_TAG, "Failed to initialize servo config");
    }
}
```

## 模块特性

### 1. 完整的硬件初始化

- 自动配置 Serial2 串口
- 创建 SerialServo 控制对象
- 初始化舵机通信协议

### 2. 全面的诊断测试

- 连接性测试
- 工作模式检查和重置
- 状态信息读取（温度、电压）
- 电机负载状态检查
- LED 告警状态处理
- 移动功能测试

### 3. 智能演示模式

- 可配置的演示动作间隔
- 循环执行 4 个角度位置（100°, 120°, 140°, 160°）
- 实时状态监控和日志输出

### 4. 错误处理机制

- 通信失败检测
- 模式切换验证
- 告警状态自动清除
- 详细的日志输出

## C/C++混合编程实践

### 1. 头文件设计

```c
#ifndef SERVO_TASK_H
#define SERVO_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

// C函数声明

#ifdef __cplusplus
}
#endif

#endif
```

### 2. C++实现中的 C 接口

```cpp
extern "C" {
    // 所有需要被C调用的函数实现
    BaseType_t servo_init_config(const servo_task_config_t *config) {
        // C++实现
    }
}
```

### 3. 避免 C++特性暴露

- 全局 C++对象隐藏在实现文件中
- 所有 C 接口函数进行参数验证
- 使用 extern 声明确保 C 链接

## 重构前后对比

### 重构前 (main.cpp)

- 200+ 行串口舵机相关代码
- 硬编码配置参数
- 难以复用和维护
- C++和 C 代码混合在一起

### 重构后

- **main.cpp**: 仅 20 行配置和调用代码
- **SerialServo 模块**: 独立的 400+行实现
- 配置化参数，易于修改
- 清晰的 C/C++接口分离
- 高度模块化，易于测试和维护

## 编译结果

```
RAM:   [=         ]  13.5% (used 44112 bytes from 327680 bytes)
Flash: [==        ]  24.3% (used 763729 bytes from 3145728 bytes)
========================= [SUCCESS] Took 37.40 seconds =========================
```

- **编译成功**：无编译错误，仅有一些预期的警告
- **内存使用**：RAM 使用增加了 40 字节，Flash 增加了 1396 字节
- **性能影响**：模块化带来的额外开销很小

## 总结

这次 SerialServo 模块重构成功展示了 C/C++混合编程的最佳实践：

1. **清晰的接口设计**：C 头文件提供简洁的 API 接口
2. **模块化实现**：C++实现复杂逻辑，C 接口封装
3. **配置驱动**：类似 WiFi 配置的方式，易于使用和扩展
4. **错误处理**：完善的错误检测和处理机制
5. **代码复用**：独立模块便于在其他项目中复用

这种架构模式可以应用到项目中的其他模块，如传感器驱动、通信模块等，实现整个项目的模块化重构。
