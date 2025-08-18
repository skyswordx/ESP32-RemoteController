# 舵机串口命令接口文档

## 概述

本文档描述了 ESP32-RemoteController 项目中新增的舵机控制串口命令接口。这些命令允许在运行时通过串口对舵机进行状态查询和控制操作。

## 命令列表

### 1. 获取舵机状态 - `servo_status`

**语法**: `servo_status <servo_id>`

**功能**: 获取指定 ID 舵机的完整状态信息

**参数**:

- `servo_id`: 舵机 ID (0-253)

**示例**:

```
servo_status 1
```

**返回信息**:

- 连接状态 (Connected/Disconnected)
- 工作模式 (Servo/Motor)
- 负载状态 (Loaded/Unloaded)
- 当前位置角度 (degrees)
- 当前速度
- 温度 (°C)
- 电压 (V)
- 最后更新时间 (ms)

### 2. 设置舵机负载状态 - `servo_load`

**语法**: `servo_load <servo_id> <0|1>`

**功能**: 切换指定 ID 舵机的负载状态

**参数**:

- `servo_id`: 舵机 ID (0-253)
- `load_state`: 负载状态
  - `0`: 卸载 (UNLOAD) - 舵机掉电，可以手动转动
  - `1`: 加载 (LOAD) - 舵机保持扭矩，不可手动转动

**示例**:

```
servo_load 1 1    # 加载舵机1
servo_load 1 0    # 卸载舵机1
```

### 3. 设置舵机工作模式 - `servo_mode`

**语法**: `servo_mode <servo_id> <0|1>`

**功能**: 切换指定 ID 舵机的工作模式

**参数**:

- `servo_id`: 舵机 ID (0-253)
- `mode`: 工作模式
  - `0`: 舵机模式 (SERVO) - 位置控制模式
  - `1`: 电机模式 (MOTOR) - 连续旋转模式

**示例**:

```
servo_mode 1 0    # 设置舵机1为舵机模式
servo_mode 1 1    # 设置舵机1为电机模式
```

### 4. 【重点】舵机位置控制 - `servo_position`

**语法**: `servo_position <servo_id> <angle> <time_ms>`

**功能**: 在舵机模式下控制舵机移动到指定角度

**参数**:

- `servo_id`: 舵机 ID (0-253)
- `angle`: 目标角度 (0.0-240.0 degrees)
- `time_ms`: 执行时间 (20-30000 ms)

注意，理论上目标角度是 0° 到 240.0°，但是在当前的机械臂抓取机构上

- 实测 100° 左右机械臂夹爪已经快要闭合
- 实测 160° 左右机械臂夹爪张开差不多达到最大张开幅度
- 实测在 100° 到 160° 之间舵机旋转角度的每一次操作用 4000 ms 是有视频中效果的

这里的舵机旋转角度和在专用串口调试软件存在下面的映射关系

```c
/* 将位置值转换为角度值，映射到 0~240° 范围
 * position_value: 专用串口助手发送的位置值 (0-1000)
 * position_angle: 在 esp32 串口终端中使用这个文档接口的命令发送的角度值 (0.0-240.0)
 */
float position_angle = (position_value / 1000.0) * 240.0;
```

**示例**:

```
servo_position 1 90.0 2000    # 舵机1在2秒内转到90度
servo_position 1 180.0 1500   # 舵机1在1.5秒内转到180度
```

### 5. 电机速度控制 - `servo_speed`

**语法**: `servo_speed <servo_id> <speed>`

**功能**: 在电机模式下设置舵机的旋转速度

**参数**:

- `servo_id`: 舵机 ID (0-253)
- `speed`: 旋转速度 (-1000 到 1000)
  - 正值: 顺时针旋转
  - 负值: 逆时针旋转
  - `0`: 停止

**示例**:

```
servo_speed 1 500     # 舵机1以中等速度顺时针旋转
servo_speed 1 -300    # 舵机1以较慢速度逆时针旋转
servo_speed 1 0       # 停止舵机1
```

## 使用流程示例

### 基本操作流程

```bash
# 1. 查看舵机状态
servo_status 1

# 2. 设置为舵机模式
servo_mode 1 0

# 3. 加载舵机
servo_load 1 1

# 4. 控制舵机转到指定位置
servo_position 1 90.0 2000

# 5. 查看执行结果
servo_status 1
```

### 电机模式操作

```bash
# 1. 设置为电机模式
servo_mode 1 1

# 2. 加载舵机
servo_load 1 1

# 3. 设置旋转速度
servo_speed 1 500

# 4. 停止旋转
servo_speed 1 0

# 5. 卸载舵机
servo_load 1 0
```

## 错误处理

当命令执行失败时，系统会通过串口返回相应的错误信息：

- `Failed to get status for servo X`: 无法获取舵机状态
- `Failed to set load state for servo X`: 无法设置负载状态
- `Failed to set work mode for servo X`: 无法设置工作模式
- `Failed to control position for servo X`: 无法控制位置
- `Failed to control speed for servo X`: 无法控制速度
- `Invalid angle/speed/time`: 参数超出有效范围
- `Servo not connected`: 舵机未连接

## 注意事项

1. **参数范围**: 所有参数都有有效范围限制，超出范围会返回错误
2. **模式切换**: 在切换工作模式前建议先卸载舵机
3. **安全操作**: 在进行位置或速度控制前确保舵机已正确连接和加载
4. **实时性**: 命令执行是实时的，建议在连续操作间添加适当延时
5. **状态查询**: 定期使用`servo_status`命令监控舵机状态

## 技术实现

- **C/C++混合编程**: 使用 C 接口封装 C++实现，确保兼容性
- **错误处理**: 完整的参数验证和错误返回机制
- **模块化设计**: 舵机控制功能独立封装在 SerialServo 模块中
- **线程安全**: 支持 FreeRTOS 多任务环境下的并发操作
