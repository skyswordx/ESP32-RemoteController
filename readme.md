<div align="center">

<br>
<img src="./img/ESP32-RemoteController.png" alt="logo" width="720"/>

# ESP32-RemoteController

![GitHub license](https://img.shields.io/github/license/skyswordx/ESP32-RemoteController)
![GitHub stars](https://img.shields.io/github/stars/skyswordx/ESP32-RemoteController)
![GitHub forks](https://img.shields.io/github/forks/skyswordx/ESP32-RemoteController)

基于 ESP32-Wroom 微控制器的项目开发平台，集成 FreeRTOS 实时操作系统和 ESP-IDF/Arduino 开发框架，提供了一个灵活的远程控制解决方案。

<div align="center">

`RAM:   13.4% (used 43760 bytes from 327680 bytes)`
`Flash: 23.3% (used 732909 bytes from 3145728 bytes)`
`Building .pio\build\esp32dev\firmware.bin`
`esptool.py v4.5.1`
</div>

</div>


## 文档概述

本文档详细说明ESP32遥控器设备的TCP数据传输协议、数据格式和接收频率特性，供TCP服务端上位机开发者参考。

---

## 网络连接配置

### **连接参数**
- **协议类型**: TCP客户端
- **默认服务器IP**: `172.26.18.126`
- **默认端口**: `2233`
- **连接超时**: 10秒
- **自动重连**: 支持

### **连接建立流程**
1. ESP32启动后自动连接WiFi网络 (SSID: "misakaa")
2. WiFi连接成功后自动发起TCP连接
3. 连接成功后发送握手消息: `"hello misakaa from esp32\n"`
4. 开始正常数据传输

---

## 发送的数据格式规范

### **1. 编码器数据格式**

```json
ENCODER:{"pos":123,"delta":1,"btn":false,"ts":12345}
```

| 字段 | 类型 | 描述 | 范围 |
|------|------|------|------|
| `pos` | `int32` | 编码器绝对位置 | -2147483648 ~ 2147483647 |
| `delta` | `int32` | 位置变化量 | -2147483648 ~ 2147483647 |
| `btn` | `boolean` | 按钮状态 | true(按下) / false(释放) |
| `ts` | `uint32` | 时间戳(FreeRTOS Tick) | 0 ~ 4294967295 |

**示例数据**:
```
ENCODER:{"pos":156,"delta":4,"btn":false,"ts":23456}
ENCODER:{"pos":160,"delta":4,"btn":true,"ts":23478}
```

### **2. 摇杆数据格式**

```json
JOYSTICK:{"x":100,"y":-50,"mag":0.85,"ang":135.0,"btn":false,"dz":false,"ts":12345}
```

| 字段 | 类型 | 描述 | 范围 |
|------|------|------|------|
| `x` | `int16` | X轴坐标值 | -512 ~ +512 |
| `y` | `int16` | Y轴坐标值 | -512 ~ +512 |
| `mag` | `float` | 摇杆偏移幅度 | 0.0 ~ 1.0 |
| `ang` | `float` | 摇杆角度(度) | 0.0 ~ 360.0 |
| `btn` | `boolean` | 按钮状态 | true(按下) / false(释放) |
| `dz` | `boolean` | 是否在死区内 | true(死区内) / false(活跃区) |
| `ts` | `uint32` | 时间戳(FreeRTOS Tick) | 0 ~ 4294967295 |

**坐标系说明**:
- X轴: 左负(-512) → 右正(+512)
- Y轴: 下负(-512) → 上正(+512)  
- 角度: 从正X轴开始逆时针测量 (0°=右, 90°=上, 180°=左, 270°=下)

**示例数据**:
```
JOYSTICK:{"x":256,"y":128,"mag":0.56,"ang":26.6,"btn":false,"dz":false,"ts":34567}
JOYSTICK:{"x":0,"y":0,"mag":0.00,"ang":0.0,"btn":true,"dz":true,"ts":34589}
```

---

## 数据传输频率分析

### **传输机制**: 事件驱动

ESP32采用**事件驱动**的数据传输机制，只在传感器状态发生变化时才发送数据，而非定时发送。

### **编码器数据频率**

#### **触发条件**
- 编码器位置发生任何变化时立即触发
- 按钮状态变化时立即触发

#### **频率特性**
| 使用场景 | 发送频率 | 说明 |
|----------|----------|------|
| 静止不动 | 0 Hz | 完全不发送数据 |
| 缓慢旋转 | 1-10 Hz | 取决于旋转速度 |
| 正常旋转 | 10-50 Hz | 典型使用场景 |
| 快速旋转 | 50-100 Hz | 受限于10ms检查周期 |
| 连续旋转 | 每刻度1次 | 物理刻度变化触发 |

#### **性能参数**
- **响应延迟**: <1ms (硬件中断)
- **最高频率**: 100Hz (理论值)
- **典型频率**: 10-30Hz (正常操作)

### **摇杆数据频率**

#### **触发条件**
- X或Y轴变化超过5像素阈值
- 进入/离开死区状态变化
- 按钮状态变化

#### **频率特性**
| 使用场景 | 发送频率 | 说明 |
|----------|----------|------|
| 静止在死区 | 0 Hz | 完全不发送数据 |
| 微小抖动 | 0 Hz | <5像素变化不触发 |
| 缓慢移动 | 5-20 Hz | 人手正常操作 |
| 快速移动 | 20-100 Hz | 快速手部动作 |
| 按钮操作 | 立即触发 | 状态变化时 |

#### **性能参数**
- **采样周期**: 10ms
- **变化阈值**: 5像素
- **死区大小**: 50像素半径
- **最高频率**: 100Hz (理论值)

### **综合传输频率**

#### **实际接收频率场景**

| 操作场景 | 预期频率 | 数据类型 |
|----------|----------|----------|
| 完全静止 | 0 Hz | 无数据 |
| 单独操作编码器 | 1-100 Hz | 仅编码器数据 |
| 单独操作摇杆 | 5-100 Hz | 仅摇杆数据 |
| 同时操作两设备 | 10-200 Hz | 两种数据交替 |
| 高强度操作 | 100-150 Hz | 实际人手限制 |

---

## AI 给的服务端开发建议

### **1. 数据解析**

```python
# Python示例代码
import json
import socket

def parse_esp32_data(data_string):
    """解析ESP32发送的数据"""
    try:
        if data_string.startswith("ENCODER:"):
            json_str = data_string[8:]  # 去掉"ENCODER:"前缀
            encoder_data = json.loads(json_str)
            return "encoder", encoder_data
            
        elif data_string.startswith("JOYSTICK:"):
            json_str = data_string[9:]  # 去掉"JOYSTICK:"前缀
            joystick_data = json.loads(json_str)
            return "joystick", joystick_data
            
    except json.JSONDecodeError:
        print(f"JSON解析错误: {data_string}")
        return None, None
    
    return None, None

# 使用示例
data_type, parsed_data = parse_esp32_data("ENCODER:{\"pos\":123,\"delta\":1,\"btn\":false,\"ts\":12345}")
if data_type == "encoder":
    position = parsed_data["pos"]
    delta = parsed_data["delta"]
    button_pressed = parsed_data["btn"]
```

### **2. 接收缓冲处理**

```python
# 处理高频数据的缓冲策略
class ESP32DataReceiver:
    def __init__(self):
        self.buffer = ""
        
    def process_received_data(self, raw_data):
        """处理接收到的原始数据"""
        self.buffer += raw_data.decode('utf-8')
        
        # 按行分割数据
        lines = self.buffer.split('\n')
        self.buffer = lines[-1]  # 保留不完整的行
        
        # 处理完整的行
        for line in lines[:-1]:
            if line.strip():
                self.handle_single_message(line.strip())
    
    def handle_single_message(self, message):
        """处理单条消息"""
        data_type, parsed_data = parse_esp32_data(message)
        if data_type:
            self.update_device_state(data_type, parsed_data)
```

### **3. 频率控制策略**

```python
import time
from collections import deque

class FrequencyController:
    def __init__(self, max_fps=60):
        self.max_fps = max_fps
        self.min_interval = 1.0 / max_fps
        self.last_update = 0
        self.data_queue = deque(maxlen=100)
    
    def should_update(self, current_time):
        """判断是否应该更新显示"""
        if current_time - self.last_update >= self.min_interval:
            self.last_update = current_time
            return True
        return False
    
    def add_data(self, data):
        """添加数据到队列"""
        self.data_queue.append((time.time(), data))
    
    def get_latest_data(self):
        """获取最新数据"""
        if self.data_queue:
            return self.data_queue[-1][1]
        return None
```

### **4. 网络异常处理**

```python
class ESP32Server:
    def __init__(self, host='0.0.0.0', port=2233):
        self.host = host
        self.port = port
        self.socket = None
        self.client_socket = None
        
    def start_server(self):
        """启动TCP服务器"""
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.socket.bind((self.host, self.port))
        self.socket.listen(1)
        print(f"服务器启动在 {self.host}:{self.port}")
        
        while True:
            try:
                self.client_socket, addr = self.socket.accept()
                print(f"ESP32设备连接: {addr}")
                self.handle_client()
            except Exception as e:
                print(f"连接错误: {e}")
                
    def handle_client(self):
        """处理客户端连接"""
        receiver = ESP32DataReceiver()
        
        try:
            while True:
                data = self.client_socket.recv(1024)
                if not data:
                    break
                receiver.process_received_data(data)
                
        except ConnectionResetError:
            print("ESP32设备断开连接")
        except Exception as e:
            print(f"数据处理错误: {e}")
        finally:
            if self.client_socket:
                self.client_socket.close()
```

---

## TODO

### **1. 数据过滤**
- 对于高频摇杆数据，建议在应用层实现额外的变化阈值过滤
- 可根据应用需求调整显示更新频率(建议30-60FPS)

### **2. 缓冲策略**
- 使用环形缓冲区存储最近的数据
- 实现数据队列管理，避免处理积压

### **3. 异常处理**
- 实现网络断线重连逻辑
- 添加数据有效性检查
- 设置合理的超时机制

### **4. 调试支持**
- 记录数据接收频率统计
- 监控网络延迟和丢包情况
- 提供数据可视化调试界面

---


## 更新记录

**文档版本**: v1.0  
**最后更新**: 2025年8月5日
