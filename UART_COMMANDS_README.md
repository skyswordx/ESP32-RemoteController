# UART 命令扩展说明

## 新增命令列表

我们已经为您的 ESP32 WiFi 远程控制器添加了以下 UART 命令，可以通过串口控制 WiFi 连接和网络协议：

### 📡 WiFi 数据链路层控制命令

#### 1. `wifi_disconnect`
- **功能**: 断开当前 WiFi 连接
- **用法**: `wifi_disconnect`
- **示例**: 
  ```
  > wifi_disconnect
  WiFi disconnected successfully.
  ```

#### 2. `wifi_connect`
- **功能**: 连接到指定的 WiFi 网络
- **用法**: `wifi_connect <ssid> [password]`
- **参数**: 
  - `ssid`: WiFi 网络名称（必须）
  - `password`: WiFi 密码（可选，开放网络可不填）
- **示例**: 
  ```
  > wifi_connect misakaa Gg114514
  Connecting to WiFi: misakaa...
  WiFi connected successfully!
  IP Address: 192.168.43.123
  ```

#### 3. `wifi_config`
- **功能**: 显示当前 WiFi 配置信息
- **用法**: `wifi_config`
- **示例**: 
  ```
  > wifi_config
  Current WiFi Configuration:
    SSID: misakaa
    Mode: Station
    Power Save: Disabled
    TX Power: 78
  ```

#### 4. `get_wifi_status`
- **功能**: 获取当前 WiFi 连接状态
- **用法**: `get_wifi_status`
- **示例**: 
  ```
  > get_wifi_status
  WiFi Status: Connected
  IP Address: 192.168.43.123
  ```

#### 5. `wifi_reconnect`
- **功能**: 使用当前保存的配置重新连接 WiFi
- **用法**: `wifi_reconnect`
- **说明**: 无需参数，会自动使用系统中当前保存的 SSID 和密码
- **示例**: 
  ```
  > wifi_reconnect
  Reconnecting to WiFi: misakaa...
  WiFi reconnected successfully!
  IP Address: 192.168.43.123
  ```

### 🌐 网络传输层控制命令

#### 5. `network_status`
- **功能**: 获取当前网络协议连接状态
- **用法**: `network_status`
- **示例**: 
  ```
  > network_status
  Network Status: Connected
  Info: TCP Client connected to 172.26.18.126:2233
  ```

#### 6. `network_disconnect`
- **功能**: 断开当前网络协议连接
- **用法**: `network_disconnect`
- **示例**: 
  ```
  > network_disconnect
  Network disconnected successfully.
  ```

#### 7. `tcp_connect`
- **功能**: 配置并连接到 TCP 服务器（ESP32 作为客户端）
- **用法**: `tcp_connect <host> <port>`
- **参数**: 
  - `host`: 服务器 IP 地址或域名
  - `port`: 服务器端口号
- **示例**: 
  ```
  > tcp_connect 172.26.18.126 2233
  Connecting to TCP server 172.26.18.126:2233...
  TCP connection established successfully!
  ```

#### 8. `network_config`
- **功能**: 显示当前网络协议配置信息
- **用法**: `network_config`
- **示例**: 
  ```
  > network_config
  Current Network Configuration:
    Protocol: TCP Client
    Remote Host: 172.26.18.126
    Remote Port: 2233
    Local Port: 0
    Auto Connect: Enabled
  ```

#### 9. `network_send`
- **功能**: 通过当前网络连接发送消息
- **用法**: `network_send <message>`
- **参数**: 
  - `message`: 要发送的消息内容（支持多个单词）
- **示例**: 
  ```
  > network_send hello from ESP32
  Message sent successfully (18 bytes).
  ```

#### 10. `network_reconnect`
- **功能**: 使用当前保存的配置重新连接网络
- **用法**: `network_reconnect`
- **说明**: 无需参数，会自动使用系统中当前保存的网络配置重新连接
- **示例**: 
  ```
  > network_reconnect
  Reconnecting to network...
  Network reconnected successfully!
  ```

### 🔧 原有系统命令

#### 11. `help`
- **功能**: 显示所有可用命令
- **用法**: `help`

#### 12. `reboot`
- **功能**: 重启 ESP32 设备
- **用法**: `reboot`

#### 13. `get_sys_info`
- **功能**: 获取系统信息
- **用法**: `get_sys_info`

## 使用场景示例

### 场景 1: 更换 WiFi 网络
```
> wifi_disconnect
WiFi disconnected successfully.

> wifi_connect "MyNewWiFi" "newpassword123"
Connecting to WiFi: MyNewWiFi...
WiFi connected successfully!
IP Address: 192.168.1.100

> get_wifi_status
WiFi Status: Connected
IP Address: 192.168.1.100
```

### 场景 2: 连接到新的 TCP 服务器
```
> network_disconnect
Network disconnected successfully.

> tcp_connect 192.168.1.50 8080
Connecting to TCP server 192.168.1.50:8080...
TCP connection established successfully!

> network_send Hello new server!
Message sent successfully (19 bytes).
```

### 场景 3: 使用默认配置快速重连
```
> wifi_reconnect
Reconnecting to WiFi: misakaa...
WiFi reconnected successfully!
IP Address: 192.168.43.123

> network_reconnect
Reconnecting to network...
Network reconnected successfully!
```

### 场景 4: 查看当前配置
```

## 注意事项

1. **网络依赖**: 网络命令需要先建立 WiFi 连接才能使用
2. **参数格式**: 
   - SSID 和密码如包含空格，请用引号包围
   - IP 地址格式：`xxx.xxx.xxx.xxx`
   - 端口范围：1-65535
3. **连接超时**: 
   - WiFi 连接超时：15 秒
   - TCP 连接超时：10 秒
4. **自动重连**: 修改配置后，原有的自动重连设置会被保留
5. **内存管理**: 断开连接时会自动释放相关资源

## 错误处理

命令执行失败时会显示相应的错误信息：
- `Usage: <command> <parameters>` - 参数错误
- `Failed to connect to WiFi` - WiFi 连接失败
- `Failed to connect to TCP server` - TCP 连接失败
- `Failed to send message. Check network connection.` - 消息发送失败

通过这些命令，您可以完全通过串口控制 ESP32 的 WiFi 连接和网络通信，非常适合远程调试和配置。
