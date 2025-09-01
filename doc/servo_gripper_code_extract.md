# main.cpp 中舵机与夹爪控制相关接口

以下内容为 `main.cpp` 中与舵机、夹爪控制相关的接口实现，便于后续重构：

---

```cpp
// 配置并初始化串口舵机控制器
servo_config_t servo_config = {
    .uart_num = 2,           // 使用Serial2
    .rx_pin = 16,            // 舵机串口RX引脚
    .tx_pin = 17,            // 舵机串口TX引脚
    .baud_rate = 115200,     // 舵机串口波特率
    .default_servo_id = 1    // 默认舵机ID
};

// 初始化舵机控制器
if (servo_controller_init(&servo_config)) {
    ESP_LOGI(MAIN_TASK_TAG, "Servo controller initialized successfully");
} else {
    ESP_LOGE(MAIN_TASK_TAG, "Failed to initialize servo controller");
}

// 初始化智能夹爪控制器
if (gripper_controller_init()) {
    ESP_LOGI(MAIN_TASK_TAG, "Gripper controller initialized successfully");
    
    // 配置默认夹爪映射 (可选 - 使用默认值时可跳过)
    gripper_mapping_t default_mapping = {
        .closed_angle = 160.0f,      // 闭合角度
        .open_angle = 90.0f,         // 张开角度
        .min_step = 5.0f,            // 最小步长
        .max_speed = 20.0f,          // 最大速度 %/s
        .is_calibrated = true,
        .reverse_direction = false
    };
    
    // 为夹爪1配置映射参数
    if (gripper_configure_mapping(1, &default_mapping)) {
        ESP_LOGI(MAIN_TASK_TAG, "Default gripper mapping configured for servo 1");
    }
    
} else {
    ESP_LOGE(MAIN_TASK_TAG, "Failed to initialize gripper controller");
}
```

---

如需进一步拆分或重构，请告知。
