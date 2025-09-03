# 3x3矩阵键盘驱动

这个库提供了ESP32上3x3矩阵键盘的驱动支持，可以检测9个按键的按下和释放状态。

## 硬件连接

```
键盘布局:
1 2 3
4 5 6
7 8 9
```

连接方式:
- 行引脚 (R1-R3): 设置为OUTPUT
- 列引脚 (C1-C3): 设置为INPUT或INPUT_PULLUP

## 使用方法

1. 初始化键盘:

```c
keypad_config_t keypad_config = {
    .row_pins = {13, 23, 22},       // 行引脚: R1, R2, R3
    .col_pins = {25, 26, 27},       // 列引脚: C1, C2, C3
    .use_pullup = true,             // 使用内部上拉电阻
    .debounce_time_ms = 20          // 去抖时间20ms
};

keypad_init(&keypad_config);
```

2. 设置按键回调函数:

```c
void on_key_event(uint8_t key, bool pressed) {
    printf("Key %d %s\n", key, pressed ? "pressed" : "released");
}

keypad_set_callback(on_key_event);
```

3. 在RTOS任务中周期性调用处理函数:

```c
void keypad_task(void* parameter) {
    while (1) {
        keypad_handler();
        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms扫描间隔
    }
}
```

4. 创建键盘任务:

```c
xTaskCreate(keypad_task, "Keypad_Task", 2048, NULL, tskIDLE_PRIORITY + 3, NULL);
```
