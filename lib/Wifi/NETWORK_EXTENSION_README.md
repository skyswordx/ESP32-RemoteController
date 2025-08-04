# ESP32 WiFi TCP 客户端扩展

## 功能概述

我们已经成功扩展了 WiFi 任务，现在支持以下网络协议：

1. **TCP 客户端模式** - ESP32 作为客户端连接到远程服务器
2. **TCP 服务端模式** - ESP32 作为服务器等待客户端连接
3. **UDP 模式** - ESP32 使用 UDP 协议进行通信

## 主要修改

### 1. WiFi 任务头文件 (lib/Wifi/wifi_task.h)

新增了以下结构和功能：

- `network_protocol_t` 枚举：定义网络协议类型
- `network_config_t` 结构体：网络协议配置
- 扩展了 `wifi_task_config_t` 结构体，添加网络配置
- 新增网络相关函数接口

### 2. WiFi 任务实现 (lib/Wifi/wifi_task.cpp)

新增功能：

- 网络任务处理函数 `network_task_handler()`
- 数据发送函数 `network_send_data()` 和 `network_send_string()`
- 网络连接状态检查 `is_network_connected()`
- 网络信息获取 `get_network_info()`

### 3. 主程序 (src/main.cpp)

配置修改：

- 配置 ESP32 为 TCP 客户端模式
- 设置远程服务器地址为 `192.168.43.1:8080` (手机热点默认网关IP)
- 在 WiFi 连接成功后自动发送 "hello misakaa from esp32" 消息

## 使用配置

### TCP 客户端模式 (当前配置)

```cpp
wifi_config.network_config.protocol = NETWORK_PROTOCOL_TCP_CLIENT;
strncpy(wifi_config.network_config.remote_host, "192.168.43.1", ...);
wifi_config.network_config.remote_port = 8080;
wifi_config.network_config.auto_connect = true;
```

### TCP 服务端模式

```cpp
wifi_config.network_config.protocol = NETWORK_PROTOCOL_TCP_SERVER;
wifi_config.network_config.local_port = 8080;
wifi_config.network_config.auto_connect = true;
```

### UDP 模式

```cpp
wifi_config.network_config.protocol = NETWORK_PROTOCOL_UDP;
wifi_config.network_config.local_port = 8080;
strncpy(wifi_config.network_config.remote_host, "192.168.43.1", ...);
wifi_config.network_config.remote_port = 8080;
wifi_config.network_config.auto_connect = true;
```

## 手机端 TCP 服务器设置

为了接收 ESP32 的消息，您需要在手机上运行一个 TCP 服务器，监听端口 8080。

您可以使用以下应用：
- **Android**: "TCP Server" 或 "Network Analyzer"
- **iOS**: "Network Analyzer" 或 "TCP Console"

或者使用简单的 Python 脚本在电脑上测试：

```python
import socket

# 创建TCP服务器
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.bind(('0.0.0.0', 8080))
server_socket.listen(1)

print("TCP服务器启动，等待ESP32连接...")

while True:
    client_socket, addr = server_socket.accept()
    print(f"ESP32已连接: {addr}")
    
    while True:
        data = client_socket.recv(1024)
        if not data:
            break
        print(f"收到消息: {data.decode('utf-8')}")
    
    client_socket.close()
```

## 注意事项

1. **IP地址**: 默认配置使用 `192.168.43.1`，这通常是手机热点的网关IP。如果不同，请修改 `remote_host`。

2. **端口**: 默认使用端口 8080，请确保手机端TCP服务器监听此端口。

3. **防火墙**: 确保手机/电脑的防火墙允许指定端口的连接。

4. **网络**: ESP32 和接收设备必须在同一个 WiFi 网络中。

## 构建和上传

使用 PlatformIO 构建和上传：

```bash
pio run                 # 构建项目
pio run --target upload # 上传到 ESP32
pio device monitor      # 监视串口输出
```

## 测试步骤

1. 在手机上开启热点 "misakaa"，密码 "Gg114514"
2. 在手机上启动 TCP 服务器监听端口 8080
3. 上传代码到 ESP32
4. 观察串口输出，确认 WiFi 连接和 TCP 连接成功
5. 在手机 TCP 服务器上应该能收到 "hello misakaa from esp32" 消息
