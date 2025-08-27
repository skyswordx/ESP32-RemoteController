# 智能夹爪控制系统使用指南

## 系统概述

新实现的智能夹爪控制系统基于PID控制器和斜坡规划器，提供了以下核心功能：

1. **丝滑百分比控制** - 使用斜坡规划器实现平滑运动
2. **精密反馈控制** - PID控制器确保精确位置跟踪
3. **多种控制模式** - 开环、闭环、力控制模式
4. **实时状态监控** - 完整的运动状态和反馈信息
5. **参数可调节** - 运行时调整控制参数

## 快速开始

### 1. 基础功能验证

#### 1.1 系统初始化验证
```bash
# ESP32上电后，串口监视器应显示：
ESP32 WiFi Task with Arduino
DataPlatform initialized successfully
Servo controller initialized successfully
Gripper controller initialized successfully
Default gripper mapping configured for servo 1
```

#### 1.2 基础夹爪控制测试
```bash
# 基础百分比控制（使用原有命令）
servo_gripper 1 50 2000          # 夹爪移动到50%位置，用时2秒

# 新的丝滑控制（推荐使用）
servo_gripper_smooth 1 30        # 夹爪丝滑移动到30%位置（自动时间）
servo_gripper_smooth 1 80 3000   # 夹爪丝滑移动到80%位置，用时3秒
```

#### 1.3 状态查询测试
```bash
# 查询夹爪状态
servo_gripper_status 1

# 预期输出示例：
========== Gripper 1 Status ==========
State: MOVING, Mode: OPEN_LOOP
Position: 45.2% (125.3°), Target: 50.0%
Moving: YES, Progress: 78.5%
Feedback: VALID, Position Error: 4.8%
Total Movements: 3, Max Error: 8.2%
Hardware Angle: 125.3°, Last Update: 150 ms ago
======================================
```

### 2. 高级功能测试

#### 2.1 控制模式切换
```bash
# 切换到闭环控制（更精确）
servo_gripper_mode 1 closed_loop

# 切换到开环控制（更快速）
servo_gripper_mode 1 open_loop

# 验证模式切换
servo_gripper_status 1
```

#### 2.2 控制参数调优
```bash
# 调整斜坡规划器和PID参数
# 格式：servo_gripper_params <servo_id> <slope_inc> <slope_dec> <pid_kp> <pid_ki> <pid_kd> <pid_limit>
servo_gripper_params 1 1.5 1.5 0.8 0.2 0.1 15.0

# 验证参数更新
servo_gripper_status 1
```

#### 2.3 映射参数配置
```bash
# 配置夹爪角度映射
servo_gripper_config 1 160 90 3

# 验证配置生效
servo_gripper_smooth 1 0    # 应移动到160°（闭合）
servo_gripper_smooth 1 100  # 应移动到90°（张开）
```

### 3. 完整测试序列

#### 3.1 精度测试流程
```bash
# 1. 初始位置设定
servo_gripper_smooth 1 0 2000
# 等待2秒

# 2. 小步长精度测试
servo_gripper_smooth 1 5     # 5%
servo_gripper_smooth 1 10    # 10%
servo_gripper_smooth 1 15    # 15%
# 每次移动后查看状态
servo_gripper_status 1

# 3. 大范围运动测试
servo_gripper_smooth 1 100 3000  # 完全张开
servo_gripper_smooth 1 0 3000    # 完全闭合
servo_gripper_smooth 1 50 2000   # 中间位置
```

#### 3.2 响应速度测试
```bash
# 测试不同时间参数的响应
servo_gripper_smooth 1 20 500    # 快速运动（500ms）
servo_gripper_smooth 1 80 1000   # 中速运动（1秒）
servo_gripper_smooth 1 40 3000   # 慢速运动（3秒）

# 测试自动时间模式
servo_gripper_smooth 1 60        # 自动计算时间
```

#### 3.3 控制模式对比测试
```bash
# 开环模式测试
servo_gripper_mode 1 open_loop
servo_gripper_smooth 1 75
servo_gripper_status 1

# 闭环模式测试
servo_gripper_mode 1 closed_loop
servo_gripper_smooth 1 25
servo_gripper_status 1
# 对比位置误差差异
```

### 4. 错误处理验证

#### 4.1 参数边界测试
```bash
# 测试参数边界
servo_gripper_smooth 1 -10       # 应报错：Invalid gripper percent
servo_gripper_smooth 1 150       # 应报错：Invalid gripper percent
servo_gripper_smooth 1 50 50     # 应报错：Invalid time
```

#### 4.2 紧急停止测试
```bash
# 启动一个长时间运动
servo_gripper_smooth 1 100 10000

# 立即停止
servo_gripper_stop 1

# 验证停止效果
servo_gripper_status 1
# State应为HOLDING，is_moving应为NO
```

### 5. 性能监控

#### 5.1 关键性能指标
监控以下指标来评估系统性能：

1. **位置精度**：位置误差 < 2%
2. **响应时间**：小变化（<10%）响应时间 < 500ms
3. **平滑性**：运动过程无明显跳跃
4. **稳定性**：反馈有效性 > 95%

#### 5.2 实时监控脚本
```bash
# 连续监控夹爪状态（手动执行）
servo_gripper_status 1
# 每隔1-2秒执行一次，观察运动过程
```

### 6. 故障排除

#### 6.1 常见问题及解决方案

**问题1：夹爪不响应**
```bash
# 检查舵机状态
servo_status 1
# 检查夹爪控制器状态
servo_gripper_status 1
```

**问题2：运动不够平滑**
```bash
# 调整斜坡规划器参数（减小步长）
servo_gripper_params 1 1.0 1.0 0.5 0.1 0.05 10.0
```

**问题3：位置误差过大**
```bash
# 切换到闭环模式并调整PID参数
servo_gripper_mode 1 closed_loop
servo_gripper_params 1 2.0 2.0 1.0 0.3 0.15 20.0
```

### 7. 高级使用场景

#### 7.1 多夹爪协调控制
```bash
# 同时控制多个夹爪（如果有）
servo_gripper_smooth 1 30
servo_gripper_smooth 2 70
# 查看两个夹爪状态
servo_gripper_status 1
servo_gripper_status 2
```

#### 7.2 精密操作序列
```bash
# 精密抓取序列
servo_gripper_mode 1 closed_loop      # 切换到高精度模式
servo_gripper_smooth 1 90 2000        # 预备位置
servo_gripper_smooth 1 60 1500        # 接近目标
servo_gripper_smooth 1 30 1000        # 轻柔抓取
servo_gripper_smooth 1 20 500         # 确保抓牢
```

### 8. 开发者调试

#### 8.1 调试信息查看
编译时添加 `-DCORE_DEBUG_LEVEL=4` 可以看到更详细的调试信息：

```bash
# ESP32控制台会显示详细的控制过程
Gripper 1 open-loop: 45.2% → 128.5° (target: 50.0%)
Gripper 1 closed-loop: plan=130.0°, feedback=128.5°, output=1.2
```

#### 8.2 性能分析
```bash
# 控制任务正常运行的标志
Control task running normally (cycle: 12000)
```

## 总结

新的智能夹爪控制系统提供了：
- ✅ 丝滑的百分比控制
- ✅ 多种控制模式选择
- ✅ 实时状态监控
- ✅ 可调节的控制参数
- ✅ 完善的错误处理

使用时建议：
1. 先使用开环模式进行基础测试
2. 根据精度要求选择合适的控制模式
3. 根据负载特性调整控制参数
4. 定期监控系统状态确保稳定运行

如有问题，请检查串口输出的日志信息进行调试。
