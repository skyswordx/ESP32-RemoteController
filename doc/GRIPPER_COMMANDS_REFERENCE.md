# 夹爪控制命令对照表

## 命令功能对比

| 功能 | 原有命令 | 新增命令 | 主要改进 |
|------|----------|----------|----------|
| 基础控制 | `servo_gripper` | `servo_gripper_smooth` | 丝滑运动，自动时间计算 |
| 状态查询 | `servo_status` | `servo_gripper_status` | 专门的夹爪状态信息 |
| 映射配置 | `servo_gripper_config` | 保持不变 | 兼容性保持 |
| 控制模式 | 无 | `servo_gripper_mode` | 开环/闭环模式选择 |
| 参数调优 | 无 | `servo_gripper_params` | 运行时参数调整 |
| 紧急停止 | 无 | `servo_gripper_stop` | 即时停止功能 |
| 校准功能 | 无 | `servo_gripper_calibrate` | 现场校准（预留） |
| 精度测试 | 无 | `servo_gripper_test` | 自动化测试（预留） |

## 详细命令说明

### 1. 基础控制命令

#### 原有命令（仍可使用）
```bash
servo_gripper <servo_id> <percent> <time_ms>
```
- 功能：一次性跳跃到目标位置
- 特点：简单直接，但运动较为生硬
- 示例：`servo_gripper 1 50 2000`

#### 新增命令（推荐使用）
```bash
servo_gripper_smooth <servo_id> <percent> [time_ms]
```
- 功能：使用斜坡规划器实现丝滑运动
- 特点：平滑过渡，可选自动时间计算
- 示例：
  - `servo_gripper_smooth 1 50` （自动时间）
  - `servo_gripper_smooth 1 50 3000` （指定时间）

### 2. 状态查询命令

#### 原有命令
```bash
servo_status <servo_id>
```
- 功能：显示舵机基础状态
- 信息：位置、温度、电压、工作模式

#### 新增命令
```bash
servo_gripper_status <servo_id>
```
- 功能：显示专门的夹爪控制状态
- 信息：控制模式、运动进度、位置误差、反馈状态等
- 示例输出：
```
========== Gripper 1 Status ==========
State: MOVING, Mode: CLOSED_LOOP
Position: 45.2% (125.3°), Target: 50.0%
Moving: YES, Progress: 78.5%
Feedback: VALID, Position Error: 4.8%
Total Movements: 3, Max Error: 8.2%
Hardware Angle: 125.3°, Last Update: 150 ms ago
======================================
```

### 3. 控制模式命令（全新功能）

```bash
servo_gripper_mode <servo_id> <mode>
```
- 模式选项：
  - `open_loop`：开环控制，快速响应
  - `closed_loop`：闭环控制，高精度
  - `force_control`：力控制（预留）
- 示例：`servo_gripper_mode 1 closed_loop`

### 4. 参数调优命令（全新功能）

```bash
servo_gripper_params <servo_id> <slope_inc> <slope_dec> <pid_kp> <pid_ki> <pid_kd> <pid_limit>
```
- 参数说明：
  - `slope_inc`：斜坡上升速率（%/周期）
  - `slope_dec`：斜坡下降速率（%/周期）
  - `pid_kp`：PID比例增益
  - `pid_ki`：PID积分增益
  - `pid_kd`：PID微分增益
  - `pid_limit`：PID输出限制
- 示例：`servo_gripper_params 1 2.0 2.0 0.8 0.2 0.1 15.0`

### 5. 紧急停止命令（全新功能）

```bash
servo_gripper_stop <servo_id>
```
- 功能：立即停止夹爪运动
- 特点：保持当前位置，切换到HOLDING状态
- 示例：`servo_gripper_stop 1`

## 使用建议

### 选择合适的控制命令

1. **日常使用**：推荐使用 `servo_gripper_smooth`
   - 运动更平滑
   - 支持自动时间计算
   - 可以调整控制精度

2. **高精度场景**：
   ```bash
   servo_gripper_mode 1 closed_loop    # 切换到闭环模式
   servo_gripper_smooth 1 50           # 执行精确控制
   ```

3. **快速响应场景**：
   ```bash
   servo_gripper_mode 1 open_loop      # 切换到开环模式
   servo_gripper_smooth 1 80           # 快速运动
   ```

4. **调试和监控**：
   ```bash
   servo_gripper_status 1              # 实时查看状态
   ```

### 参数调优指南

1. **运动太慢**：增大 `slope_inc` 和 `slope_dec`
2. **运动不稳定**：减小PID增益参数
3. **精度不够**：切换到闭环模式，调整PID参数
4. **响应过慢**：切换到开环模式或增大PID增益

### 兼容性说明

- 所有原有命令仍然可用
- 新增命令不会影响现有功能
- 可以混合使用新旧命令
- 建议逐步迁移到新命令以获得更好的控制效果

## 故障排除

| 问题现象 | 可能原因 | 解决方案 |
|----------|----------|----------|
| 夹爪不响应 | 控制器未初始化 | 检查启动日志，重启系统 |
| 运动不平滑 | 斜坡参数过大 | 减小 `slope_inc` 和 `slope_dec` |
| 位置误差大 | 开环模式精度限制 | 切换到闭环模式 |
| 反馈无效 | 硬件连接问题 | 检查舵机连接和电源 |
| 系统卡死 | 参数设置不当 | 使用 `servo_gripper_stop` 停止运动 |
