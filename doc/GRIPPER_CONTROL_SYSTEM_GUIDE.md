# 夹爪控制系统实现指导文档

## 用户需求分析

### 核心需求
1. **丝滑控制**：通过 `servo_gripper 1 xx 1000` 命令实现夹爪张开百分比的平滑控制
2. **精密控制**：实现最小LSB级别的精度控制，克服机械阻力
3. **实时反馈**：基于舵机码盘值（0-240°）的实时位置反馈
4. **现场校准**：提供微调接口处理机械装配误差

### 当前系统限制
1. **控制粗糙**：一次性跳跃到目标位置，缺乏平滑性
2. **小变化无响应**：5%变化（约3.5°）可能被机械死区吞没
3. **状态不准确**：缺乏实时位置状态维护
4. **死区处理不当**：15°最小步长对精细控制过大

---

## 系统架构设计

### 1. 分层控制架构
```
应用层：UART命令解析 (servo_gripper 1 xx 1000)
       ↓
控制层：斜坡规划器 + 阻力补偿
       ↓
执行层：力矩控制 + 位置反馈
       ↓
硬件层：串口舵机 (0-240° 码盘值)
```

### 2. 核心组件

#### A. 状态管理器
```cpp
typedef struct {
    uint8_t servo_id;
    float current_percent;      // 当前百分比位置 (0-100)
    float target_percent;       // 目标百分比位置
    float current_angle;        // 对应的实际角度
    float hardware_angle;       // 实时读取的码盘值
    bool is_moving;            // 是否正在运动
    uint32_t last_update;      // 上次更新时间
    bool read_valid;           // 码盘读取是否有效
} gripper_state_t;
```

#### B. 角度映射系统
```cpp
typedef struct {
    float closed_angle;         // 闭合状态角度 (如160°)
    float open_angle;          // 张开状态角度 (如90°)
    float min_step;            // 最小有效步长
    bool is_calibrated;        // 是否已校准
} gripper_mapping_t;
```

#### C. 摩擦力模型
```cpp
typedef struct {
    float static_friction_torque;    // 静摩擦力矩
    float kinetic_friction_coeff;    // 动摩擦系数
    float gear_backlash_angle;       // 齿轮间隙角度
    float load_torque_estimate;      // 负载力矩估计
    float adaptive_gain;             // 自适应增益
} friction_model_t;
```

---

## 核心算法实现

### 1. 实时百分比获取
```cpp
float get_current_gripper_percent(uint8_t servo_id) {
    // 方案1：实时读取码盘值（推荐）
    int actual_angle = LobotSerialServoReadPosition(servo_id);
    if (actual_angle >= 0) {
        gripper_mapping_t* mapping = get_gripper_mapping(servo_id);
        return angle_to_percent(actual_angle, mapping);
    }
    
    // 方案2：使用软件状态估计（备用）
    gripper_state_t* state = get_gripper_state(servo_id);
    if (state->read_valid && (millis() - state->last_read_time < 5000)) {
        return state->software_percent;
    }
    
    return -1; // 读取失败
}

float angle_to_percent(float angle, gripper_mapping_t* mapping) {
    // 角度映射：160°(闭合)=0%, 90°(张开)=100%
    float percent = (mapping->closed_angle - angle) / 
                   (mapping->closed_angle - mapping->open_angle) * 100.0;
    return constrain(percent, 0.0, 100.0);
}
```

### 2. 斜坡规划器
```cpp
void gripper_interpolation_task(void *param) {
    while(1) {
        for(int i = 0; i < MAX_GRIPPERS; i++) {
            gripper_state_t* gripper = &gripper_states[i];
            
            if(gripper->is_moving) {
                // 计算下一步位置
                float next_percent = calculate_next_step(gripper);
                
                // 执行带前馈补偿的运动
                execute_gripper_movement(gripper->servo_id, next_percent);
                
                // 更新状态
                update_gripper_state(gripper, next_percent);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // 50ms间隔，20Hz控制频率
    }
}

float calculate_next_step(gripper_state_t* gripper) {
    float diff = gripper->target_percent - gripper->current_percent;
    float max_step_per_cycle = 2.0; // 每周期最大2%变化
    
    if(fabs(diff) <= max_step_per_cycle) {
        gripper->is_moving = false;
        return gripper->target_percent; // 到达目标
    } else {
        return gripper->current_percent + 
               (diff > 0 ? max_step_per_cycle : -max_step_per_cycle);
    }
}
```

### 3. 阻力前馈补偿
```cpp
bool execute_gripper_movement(uint8_t servo_id, float target_percent) {
    gripper_mapping_t* mapping = get_gripper_mapping(servo_id);
    friction_model_t* friction = get_friction_model(servo_id);
    
    // 转换为角度
    float target_angle = percent_to_angle(target_percent, mapping);
    float current_angle = get_current_angle(servo_id);
    
    // 计算前馈力矩
    float ff_torque = calculate_feedforward_torque(target_angle, current_angle, friction);
    
    // 小步长精密控制
    float step_size = (fabs(target_angle - current_angle) > 5.0) ? 2.0 : 0.5;
    float intermediate_angle = current_angle + 
        ((target_angle > current_angle) ? step_size : -step_size);
    
    // 执行运动（需要舵机支持力矩控制）
    return servo_control_with_feedforward(servo_id, intermediate_angle, ff_torque);
}

float calculate_feedforward_torque(float target_angle, float current_angle, 
                                 friction_model_t* model) {
    float direction = (target_angle > current_angle) ? 1.0 : -1.0;
    
    // 静摩擦补偿
    float ff_torque = model->static_friction_torque * direction;
    
    // 负载补偿
    ff_torque += model->load_torque_estimate;
    
    // 间隙补偿
    if(direction != get_last_direction()) {
        ff_torque += model->gear_backlash_angle * model->adaptive_gain;
    }
    
    return ff_torque;
}
```

---

## 命令接口设计

### 1. 基础控制命令
```bash
# 丝滑百分比控制
servo_gripper <servo_id> <percent> <time_ms>
# 示例：servo_gripper 1 30 1000  # 1号夹爪张开到30%，用时1秒

# 配置映射参数
servo_gripper_config <servo_id> <closed_angle> <open_angle> <min_step>
# 示例：servo_gripper_config 1 160 90 5
```

### 2. 校准命令
```bash
# 现场校准（将当前位置设为参考点）
servo_gripper_calibrate <servo_id> <position>
# 示例：
# servo_gripper_calibrate 1 closed  # 设置当前位置为闭合状态
# servo_gripper_calibrate 1 open    # 设置当前位置为张开状态

# 微调命令
servo_gripper_adjust <servo_id> <position> <offset>
# 示例：
# servo_gripper_adjust 1 closed +2  # 闭合角度微调+2°
# servo_gripper_adjust 1 open -1    # 张开角度微调-1°

# 保存配置
servo_gripper_save <servo_id>      # 保存当前映射到EEPROM
```

### 3. 诊断命令
```bash
# 状态查询
servo_gripper_status <servo_id>    # 显示当前百分比、角度、运动状态

# 摩擦力学习
servo_gripper_learn <servo_id>     # 执行摩擦参数自动学习

# 精度测试
servo_gripper_test <servo_id> <start> <end> <step>
# 示例：servo_gripper_test 1 0 100 1  # 从0%到100%，步长1%测试
```

---

## 实现路线图

### 阶段1：基础丝滑控制
1. **添加状态管理器**：维护每个夹爪的实时状态
2. **创建斜坡规划器任务**：50ms间隔的插值控制
3. **修改命令处理**：只设置目标，不直接移动
4. **测试验证**：确保小百分比变化能累积成可见运动

### 阶段2：精密控制优化
1. **实现实时读取**：基于码盘值的位置反馈
2. **添加校准接口**：现场调整映射参数
3. **优化死区处理**：动态调整最小步长
4. **性能测试**：验证1%精度控制能力

### 阶段3：LSB级别控制（高级功能）
1. **实现阻力前馈**：静摩擦和负载补偿
2. **添加自适应学习**：自动优化摩擦参数
3. **力矩控制模式**：结合位置和力矩控制
4. **精度验证**：达到理论LSB控制精度

### 阶段4：系统集成优化
1. **容错处理**：码盘读取失败的备用方案
2. **性能优化**：减少通信延迟和计算负担
3. **参数持久化**：EEPROM存储校准数据
4. **用户界面**：完善的诊断和调试命令

---

## 关键技术要点

### 1. 控制频率选择
- **规划器频率**：20Hz (50ms) - 平衡响应性和计算负担
- **反馈频率**：10Hz (100ms) - 码盘读取不宜过频繁
- **命令接收**：异步处理，立即更新目标值

### 2. 精度与稳定性平衡
- **大变化**：2%/周期，快速响应
- **小变化**：0.5%/周期，精密控制
- **死区处理**：动态调整，避免振荡

### 3. 错误处理策略
- **通信失败**：使用软件估计值
- **位置异常**：限制最大变化率
- **力矩饱和**：自适应降低增益

### 4. 性能指标
- **响应时间**：小于200ms（感知丝滑）
- **控制精度**：±1%（基础需求）
- **稳态误差**：<0.5%（精密控制）

---

## 总结

本指导文档提供了从当前基础位置控制升级到高精度丝滑夹爪控制系统的完整解决方案。通过分阶段实施，可以逐步达到用户的控制精度和响应性需求。核心是通过状态管理、斜坡规划和阻力补偿三个层次的协同工作，实现真正意义上的精密夹爪控制。
