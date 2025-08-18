# TODO

- [x] 把 `encoder_task()` 和 `joystick_task()` 的实现替换成真正的 FreeRTOS 任务
- [ ] 添加类似uart命令解析表的在 `wifi_task.cpp` 中的网络任务处理函数
- [x] 添加串口舵机驱动的支持
- [ ] 添加蓝牙串口的支持
- [x] 完善供电方案
- [x] 引脚表
    - D16、D17: 舵机串口RX/TX，和一个按键 + 编码器按键冲突

