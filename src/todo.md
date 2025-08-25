# TODO

- [x] 把 `encoder_task()` 和 `joystick_task()` 的实现替换成真正的 FreeRTOS 任务
- [ ] 添加类似uart命令解析表的在 `wifi_task.cpp` 中的网络任务处理函数
- [x] 添加串口舵机驱动的支持
- [ ] 添加蓝牙串口的支持
- [x] 完善供电方案
- [x] 引脚表
    - D16、D17: 舵机串口RX/TX，和一个按键 + 编码器按键冲突
- [x] 简化串口舵机控制系统：
    - [x] 去除demo演示功能 
    - [x] 去除servo task（改为直接函数调用）
    - [x] 去除中间封装层（直接使用SerialServo对象）
    - [x] 改进诊断测试（使用100°→160°实用角度范围）

