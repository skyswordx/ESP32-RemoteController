#include "wifi_task.h"
#include "WiFi.h"

#define WIFI_TASK_TAG "WIFI_TASK"
#define NETWORK_TASK_TAG "NETWORK_TASK"

// 声明外部函数，用于将TCP接收到的命令发送到解析队列
extern "C" int uart_parser_send_command_to_queue(char *cmd_string);

static wifi_task_config_t* s_wifi_config = NULL;
static bool s_is_connected = false;
static bool s_network_connected = false;

// 网络对象
static WiFiClient* s_tcp_client = NULL;
static WiFiServer* s_tcp_server = NULL;
static WiFiUDP* s_udp = NULL;
static char s_network_info[256] = {0};

// 网络任务处理函数声明
static void my_network_task(void *pvParameters);

// 网络数据接收处理任务声明
static void network_receive_task(void *pvParameters);

// WiFi 初始化配置函数
BaseType_t wifi_init_config(wifi_task_config_t *config)
{
    if (config == NULL) {
        return pdFAIL;
    }

    // Copy config to a static variable to be safely accessed by the handler
    static wifi_task_config_t task_config;
    memcpy(&task_config, config, sizeof(wifi_task_config_t));
    s_wifi_config = &task_config;

    return pdPASS;
}

// WiFi 处理函数 (不再是任务函数)
void wifi_handler(void)
{
    static bool wifi_initialized = false;
    
    if (!wifi_initialized && s_wifi_config != NULL) {
        // A small delay to help prevent brownout if power supply is marginal
        vTaskDelay(pdMS_TO_TICKS(200));

        ESP_LOGI(WIFI_TASK_TAG, "Starting WiFi initialization...");

        // Set WiFi Mode
        WiFi.mode(s_wifi_config->wifi_mode);
    ESP_LOGI(WIFI_TASK_TAG, "WiFi mode set to: %d", s_wifi_config->wifi_mode);

    // Set power save mode
    // Note: In Arduino, this is typically managed with WiFi.setSleep(true/false)
    // We will map the boolean flag to this.
    WiFi.setSleep(s_wifi_config->power_save);
    ESP_LOGI(WIFI_TASK_TAG, "Power save mode: %s", s_wifi_config->power_save ? "ON" : "OFF");

    // Set transmit power
    WiFi.setTxPower(s_wifi_config->tx_power);
    ESP_LOGI(WIFI_TASK_TAG, "TX Power set to: %d", (int)s_wifi_config->tx_power);


    if (s_wifi_config->wifi_mode == WIFI_STA || s_wifi_config->wifi_mode == WIFI_AP_STA) {
        ESP_LOGI(WIFI_TASK_TAG, "Connecting to STA: %s", s_wifi_config->ssid);
        WiFi.begin(s_wifi_config->ssid, s_wifi_config->password);

        uint32_t start_time = millis();
        while (WiFi.status() != WL_CONNECTED) {
            vTaskDelay(pdMS_TO_TICKS(500));
            ESP_LOGI(WIFI_TASK_TAG, ".");
            if (millis() - start_time > s_wifi_config->sta_connect_timeout_ms) {
                ESP_LOGE(WIFI_TASK_TAG, "Connection Timeout!");
                break;
            }
        }
    }

    if (s_wifi_config->wifi_mode == WIFI_AP || s_wifi_config->wifi_mode == WIFI_AP_STA) {
        ESP_LOGI(WIFI_TASK_TAG, "Starting AP: %s", s_wifi_config->ap_ssid);
        WiFi.softAP(s_wifi_config->ap_ssid, s_wifi_config->ap_password);
        IPAddress myIP = WiFi.softAPIP();
        ESP_LOGI(WIFI_TASK_TAG, "AP IP address: %s", myIP.toString().c_str());
    }

    if (WiFi.status() == WL_CONNECTED) {
        s_is_connected = true;
        ESP_LOGI(WIFI_TASK_TAG, "WiFi Connected. IP Address: %s", WiFi.localIP().toString().c_str());
        
        // 如果配置了网络协议且需要自动连接，启动网络任务
        if (s_wifi_config->network_config.protocol != NETWORK_PROTOCOL_NONE && 
            s_wifi_config->network_config.auto_connect) {
            ESP_LOGI(WIFI_TASK_TAG, "Starting network task...");
            if (xTaskCreate(my_network_task, "network_task", 4096, NULL, 4, NULL) != pdPASS) {
                ESP_LOGE(WIFI_TASK_TAG, "Failed to create network task");
            }
        }
        wifi_initialized = true;
    } else if (!wifi_initialized) {
        s_is_connected = false;
        ESP_LOGW(WIFI_TASK_TAG, "WiFi connection failed or not in STA mode.");
        wifi_initialized = true; // 标记已尝试初始化，避免重复
    }
    } // 关闭 if (!wifi_initialized && s_wifi_config != NULL) 的大括号
}

bool is_wifi_connected(void)
{
    // For STA mode, we can rely on WiFi.isConnected()
    s_is_connected = WiFi.isConnected();
    return s_is_connected;
}

// 网络任务监控标志
static volatile bool network_task_running = true;
static volatile bool network_reconnect_in_progress = false;

// 网络任务处理函数
static void my_network_task(void *pvParameters)
{
    ESP_LOGI(NETWORK_TASK_TAG, "Starting network task...");
    
    network_config_t* net_config = &s_wifi_config->network_config;
    bool initial_connection_done = false;
    
    // 持续监控和维护网络连接
    while (network_task_running) {
        // 首次连接初始化
        if (!initial_connection_done) {
            switch (net_config->protocol) {
                case NETWORK_PROTOCOL_TCP_CLIENT:
                {
                    ESP_LOGI(NETWORK_TASK_TAG, "Initializing TCP Client mode");
                    s_tcp_client = new WiFiClient();
                    
                    uint32_t start_time = millis();
                    bool connection_timeout = false;
                    
                    while (!s_tcp_client->connected() && !connection_timeout) {
                        ESP_LOGI(NETWORK_TASK_TAG, "Connecting to TCP server %s:%d", 
                                net_config->remote_host, net_config->remote_port);
                        
                        if (s_tcp_client->connect(net_config->remote_host, net_config->remote_port)) {
                            s_network_connected = true;
                            ESP_LOGI(NETWORK_TASK_TAG, "TCP Client connected successfully");
                            snprintf(s_network_info, sizeof(s_network_info), 
                                    "TCP Client connected to %s:%d", 
                                    net_config->remote_host, net_config->remote_port);
                            
                            // 创建网络数据接收任务
                            xTaskCreate(network_receive_task, "network_rx_task", 4096, NULL, 5, NULL);
                            break;
                        } else {
                            ESP_LOGW(NETWORK_TASK_TAG, "TCP connection failed, retrying...");
                            vTaskDelay(pdMS_TO_TICKS(1000));
                        }
                        
                        if (millis() - start_time > net_config->connect_timeout_ms) {
                            ESP_LOGE(NETWORK_TASK_TAG, "TCP connection timeout!");
                            connection_timeout = true;
                        }
                    }
                    break;
                }
                
                case NETWORK_PROTOCOL_TCP_SERVER:
                {
                    ESP_LOGI(NETWORK_TASK_TAG, "Initializing TCP Server mode on port %d", net_config->local_port);
                    s_tcp_server = new WiFiServer(net_config->local_port);
                    s_tcp_server->begin();
                    s_network_connected = true;
                    ESP_LOGI(NETWORK_TASK_TAG, "TCP Server started successfully");
                    snprintf(s_network_info, sizeof(s_network_info), 
                            "TCP Server listening on port %d", net_config->local_port);
                    
                    // 创建网络数据接收任务
                    xTaskCreate(network_receive_task, "network_rx_task", 4096, NULL, 5, NULL);
                    break;
                }
                
                case NETWORK_PROTOCOL_UDP:
                {
                    ESP_LOGI(NETWORK_TASK_TAG, "Initializing UDP mode on port %d", net_config->local_port);
                    s_udp = new WiFiUDP();
                    if (s_udp->begin(net_config->local_port)) {
                        s_network_connected = true;
                        ESP_LOGI(NETWORK_TASK_TAG, "UDP initialized successfully");
                        snprintf(s_network_info, sizeof(s_network_info), 
                                "UDP listening on port %d", net_config->local_port);
                        
                        // 创建网络数据接收任务
                        xTaskCreate(network_receive_task, "network_rx_task", 4096, NULL, 5, NULL);
                    } else {
                        ESP_LOGE(NETWORK_TASK_TAG, "Failed to initialize UDP");
                    }
                    break;
                }
                
                default:
                    ESP_LOGW(NETWORK_TASK_TAG, "Unknown network protocol");
                    break;
            }
            
            initial_connection_done = true;
        }
        // 连接建立后持续监控
        else {
            // 仅当WiFi连接正常时检查网络连接状态
            if (is_wifi_connected()) {
                if (!is_network_connected() && !network_reconnect_in_progress) {
                    ESP_LOGW(NETWORK_TASK_TAG, "Network connection lost, attempting to reconnect");
                    network_reconnect_in_progress = true;
                    
                    // 根据当前配置重新建立网络连接
                    switch (net_config->protocol) {
                        case NETWORK_PROTOCOL_TCP_CLIENT:
                            ESP_LOGI(NETWORK_TASK_TAG, "Reconnecting TCP client to %s:%d", 
                                    net_config->remote_host, net_config->remote_port);
                                    
                            // 先断开旧连接
                            network_disconnect();
                            vTaskDelay(pdMS_TO_TICKS(1000)); // 等待断开完成
                            
                            // 尝试重新连接
                            if (network_connect_tcp_client(
                                    net_config->remote_host, 
                                    net_config->remote_port, 
                                    net_config->connect_timeout_ms)) {
                                ESP_LOGI(NETWORK_TASK_TAG, "TCP Client reconnected successfully");
                            } else {
                                ESP_LOGE(NETWORK_TASK_TAG, "Failed to reconnect TCP Client");
                            }
                            break;
                            
                        case NETWORK_PROTOCOL_TCP_SERVER:
                            // TCP服务器模式下，如果服务器对象被销毁，尝试重新创建
                            if (s_tcp_server == NULL) {
                                ESP_LOGI(NETWORK_TASK_TAG, "Restarting TCP Server on port %d", net_config->local_port);
                                s_tcp_server = new WiFiServer(net_config->local_port);
                                s_tcp_server->begin();
                                s_network_connected = true;
                                
                                // 创建网络数据接收任务
                                xTaskCreate(network_receive_task, "network_rx_task", 4096, NULL, 5, NULL);
                            }
                            break;
                            
                        case NETWORK_PROTOCOL_UDP:
                            // UDP模式下，如果UDP对象被销毁，尝试重新创建
                            if (s_udp == NULL) {
                                ESP_LOGI(NETWORK_TASK_TAG, "Restarting UDP on port %d", net_config->local_port);
                                s_udp = new WiFiUDP();
                                if (s_udp->begin(net_config->local_port)) {
                                    s_network_connected = true;
                                    
                                    // 创建网络数据接收任务
                                    xTaskCreate(network_receive_task, "network_rx_task", 4096, NULL, 5, NULL);
                                }
                            }
                            break;
                            
                        default:
                            break;
                    }
                    
                    network_reconnect_in_progress = false;
                }
            }
        }
        
        // 每3秒检查一次网络连接状态
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    
    // 如果任务被请求停止
    ESP_LOGI(NETWORK_TASK_TAG, "Network task stopping");
    vTaskDelete(NULL);
}

int network_send_data(const uint8_t* data, size_t len)
{
    if (!s_network_connected || data == NULL || len == 0) {
        return -1;
    }
    
    network_config_t* net_config = &s_wifi_config->network_config;
    
    switch (net_config->protocol) {
        case NETWORK_PROTOCOL_TCP_CLIENT:
            if (s_tcp_client && s_tcp_client->connected()) {
                return s_tcp_client->write(data, len);
            }
            break;
            
        case NETWORK_PROTOCOL_TCP_SERVER:
            // TCP 服务器模式下，需要向所有连接的客户端发送数据
            if (s_tcp_server) {
                WiFiClient client = s_tcp_server->available();
                if (client) {
                    return client.write(data, len);
                }
            }
            break;
            
        case NETWORK_PROTOCOL_UDP:
            if (s_udp) {
                s_udp->beginPacket(net_config->remote_host, net_config->remote_port);
                int result = s_udp->write(data, len);
                s_udp->endPacket();
                return result;
            }
            break;
            
        default:
            break;
    }
    
    return -1;
}

int network_send_string(const char* str)
{
    if (str == NULL) {
        return -1;
    }
    return network_send_data((const uint8_t*)str, strlen(str));
}

bool is_network_connected(void)
{
    if (!s_network_connected) {
        return false;
    }
    
    network_config_t* net_config = &s_wifi_config->network_config;
    
    switch (net_config->protocol) {
        case NETWORK_PROTOCOL_TCP_CLIENT:
            return (s_tcp_client && s_tcp_client->connected());
            
        case NETWORK_PROTOCOL_TCP_SERVER:
            return (s_tcp_server != NULL);
            
        case NETWORK_PROTOCOL_UDP:
            return (s_udp != NULL);
            
        default:
            return false;
    }
}

const char* get_network_info(void)
{
    return s_network_info;
}

bool wifi_disconnect(void)
{
    ESP_LOGI(WIFI_TASK_TAG, "Disconnecting WiFi...");
    WiFi.disconnect();
    s_is_connected = false;
    return true;
}

bool wifi_connect_new(const char* ssid, const char* password, uint32_t timeout_ms)
{
    if (ssid == NULL) {
        ESP_LOGE(WIFI_TASK_TAG, "SSID cannot be NULL");
        return false;
    }
    
    ESP_LOGI(WIFI_TASK_TAG, "Connecting to new WiFi: %s", ssid);
    
    // 断开当前连接
    WiFi.disconnect();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 连接新网络
    if (password && strlen(password) > 0) {
        WiFi.begin(ssid, password);
    } else {
        WiFi.begin(ssid);
    }
    
    uint32_t start_time = millis();
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
        if (millis() - start_time > timeout_ms) {
            ESP_LOGE(WIFI_TASK_TAG, "WiFi connection timeout");
            s_is_connected = false;
            return false;
        }
    }
    
    s_is_connected = true;
    ESP_LOGI(WIFI_TASK_TAG, "WiFi connected successfully. IP: %s", WiFi.localIP().toString().c_str());
    
    // 更新配置
    if (s_wifi_config) {
        strncpy(s_wifi_config->ssid, ssid, sizeof(s_wifi_config->ssid) - 1);
        s_wifi_config->ssid[sizeof(s_wifi_config->ssid) - 1] = '\0';
        
        if (password) {
            strncpy(s_wifi_config->password, password, sizeof(s_wifi_config->password) - 1);
            s_wifi_config->password[sizeof(s_wifi_config->password) - 1] = '\0';
        } else {
            s_wifi_config->password[0] = '\0';
        }
    }
    
    return true;
}

bool get_current_wifi_config(wifi_task_config_t* config)
{
    if (config == NULL || s_wifi_config == NULL) {
        return false;
    }
    
    memcpy(config, s_wifi_config, sizeof(wifi_task_config_t));
    return true;
}

bool network_disconnect(void)
{
    ESP_LOGI(NETWORK_TASK_TAG, "Disconnecting network...");
    
    if (s_tcp_client) {
        s_tcp_client->stop();
        delete s_tcp_client;
        s_tcp_client = NULL;
    }
    
    if (s_tcp_server) {
        s_tcp_server->end();
        delete s_tcp_server;
        s_tcp_server = NULL;
    }
    
    if (s_udp) {
        s_udp->stop();
        delete s_udp;
        s_udp = NULL;
    }
    
    s_network_connected = false;
    memset(s_network_info, 0, sizeof(s_network_info));
    
    ESP_LOGI(NETWORK_TASK_TAG, "Network disconnected");
    return true;
}

bool network_connect_tcp_client(const char* remote_host, uint16_t remote_port, uint32_t timeout_ms)
{
    if (remote_host == NULL) {
        ESP_LOGE(NETWORK_TASK_TAG, "Remote host cannot be NULL");
        return false;
    }
    
    // 断开当前连接
    network_disconnect();
    
    ESP_LOGI(NETWORK_TASK_TAG, "Connecting TCP client to %s:%d", remote_host, remote_port);
    
    s_tcp_client = new WiFiClient();
    
    uint32_t start_time = millis();
    while (!s_tcp_client->connected()) {
        if (s_tcp_client->connect(remote_host, remote_port)) {
            s_network_connected = true;
            ESP_LOGI(NETWORK_TASK_TAG, "TCP Client connected successfully");
            snprintf(s_network_info, sizeof(s_network_info), 
                    "TCP Client connected to %s:%d", remote_host, remote_port);
            
            // 创建网络数据接收任务
            xTaskCreate(network_receive_task, "network_rx_task", 4096, NULL, 5, NULL);
            
            // 更新配置
            if (s_wifi_config) {
                s_wifi_config->network_config.protocol = NETWORK_PROTOCOL_TCP_CLIENT;
                strncpy(s_wifi_config->network_config.remote_host, remote_host, 
                       sizeof(s_wifi_config->network_config.remote_host) - 1);
                s_wifi_config->network_config.remote_host[sizeof(s_wifi_config->network_config.remote_host) - 1] = '\0';
                s_wifi_config->network_config.remote_port = remote_port;
            }
            
            return true;
        } else {
            vTaskDelay(pdMS_TO_TICKS(1000));
            if (millis() - start_time > timeout_ms) {
                ESP_LOGE(NETWORK_TASK_TAG, "TCP connection timeout");
                delete s_tcp_client;
                s_tcp_client = NULL;
                return false;
            }
        }
    }
    
    return false;
}

bool get_current_network_config(network_config_t* config)
{
    if (config == NULL || s_wifi_config == NULL) {
        return false;
    }
    
    memcpy(config, &s_wifi_config->network_config, sizeof(network_config_t));
    return true;
}

/**
 * @brief 重新启动网络监控系统
 * 
 * 这个函数重置并重新启动整个网络监控系统，包括WiFi连接和网络协议连接。
 * 在网络状态异常时可以调用此函数进行全面的恢复。
 * 
 * @return true 如果重启成功
 * @return false 如果重启失败
 */
bool restart_network_system()
{
    ESP_LOGI(NETWORK_TASK_TAG, "Restarting network system...");
    
    // 1. 断开网络连接
    network_disconnect();
    
    // 2. 断开WiFi连接
    wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(1000)); // 等待断开完成
    
    // 3. 获取当前配置
    wifi_task_config_t config;
    if (!get_current_wifi_config(&config)) {
        ESP_LOGE(NETWORK_TASK_TAG, "Failed to get WiFi configuration");
        return false;
    }
    
    // 4. 重新连接WiFi
    if (!wifi_connect_new(config.ssid, config.password, config.sta_connect_timeout_ms)) {
        ESP_LOGE(NETWORK_TASK_TAG, "Failed to reconnect WiFi");
        return false;
    }
    
    // 5. 如果配置了网络协议，重新建立网络连接
    if (config.network_config.protocol != NETWORK_PROTOCOL_NONE) {
        // 根据协议类型重新连接
        switch (config.network_config.protocol) {
            case NETWORK_PROTOCOL_TCP_CLIENT:
                if (!network_connect_tcp_client(
                        config.network_config.remote_host,
                        config.network_config.remote_port,
                        config.network_config.connect_timeout_ms)) {
                    ESP_LOGE(NETWORK_TASK_TAG, "Failed to reconnect TCP client");
                    return false;
                }
                break;
                
            // 可以添加其他网络协议类型的处理
            
            default:
                break;
        }
    }
    
    ESP_LOGI(NETWORK_TASK_TAG, "Network system restarted successfully");
    return true;
}

// 网络数据接收处理任务
static void network_receive_task(void *pvParameters)
{
    ESP_LOGI(NETWORK_TASK_TAG, "Network receive task started");
    
    char rx_buffer[256]; // 接收缓冲区
    static char cmd_buffer[256]; // 命令缓冲区
    static int cmd_index = 0;
    
    while (1) {
        // 根据当前网络协议类型处理数据接收
        network_config_t* net_config = &s_wifi_config->network_config;
        int bytes_received = 0;
        
        switch (net_config->protocol) {
            case NETWORK_PROTOCOL_TCP_CLIENT:
                if (s_tcp_client && s_tcp_client->connected()) {
                    bytes_received = s_tcp_client->available();
                    if (bytes_received > 0) {
                        // 限制单次读取大小
                        int read_size = bytes_received > sizeof(rx_buffer) - 1 ? sizeof(rx_buffer) - 1 : bytes_received;
                        s_tcp_client->read((uint8_t*)rx_buffer, read_size);
                        rx_buffer[read_size] = '\0';
                        
                        ESP_LOGI(NETWORK_TASK_TAG, "TCP Received %d bytes: %s", read_size, rx_buffer);
                        
                        // 处理接收到的数据，转发给命令解析系统
                        for (int i = 0; i < read_size; i++) {
                            char c = rx_buffer[i];
                            
                            // 检测命令结束符 (回车或换行)
                            if (c == '\r' || c == '\n') {
                                if (cmd_index > 0) {
                                    // 添加字符串结束符
                                    cmd_buffer[cmd_index] = '\0';
                                    
                                    ESP_LOGI(NETWORK_TASK_TAG, "Processing command from TCP: %s", cmd_buffer);
                                    
                                    // 将命令发送到解析队列
                                    static char *cmd_copy = NULL;
                                    // 释放之前的内存
                                    if (cmd_copy != NULL) {
                                        free(cmd_copy);
                                        cmd_copy = NULL;
                                    }
                                    // 分配内存并复制命令
                                    cmd_copy = (char *)malloc(cmd_index + 1);
                                    if (cmd_copy) {
                                        strcpy(cmd_copy, cmd_buffer);
                                        // 将命令送入队列
                                        if (uart_parser_send_command_to_queue(cmd_copy) != pdPASS) {
                                            ESP_LOGW(NETWORK_TASK_TAG, "Command queue is full, command discarded");
                                            free(cmd_copy);
                                            cmd_copy = NULL;
                                        }
                                    }
                                    
                                    // 重置命令缓冲区索引
                                    cmd_index = 0;
                                }
                            } 
                            // 处理字符串形式的"\r"或"\n"
                            else if (c == '\\' && i + 1 < read_size) {
                                // 检查下一个字符
                                char next_c = rx_buffer[i + 1];
                                if (next_c == 'r' || next_c == 'n') {
                                    // 跳过'\\'，直接处理为结束符
                                    i++; // 跳过下一个字符('r'或'n')
                                    
                                    if (cmd_index > 0) {
                                        // 添加字符串结束符
                                        cmd_buffer[cmd_index] = '\0';
                                        
                                        ESP_LOGI(NETWORK_TASK_TAG, "Processing command from TCP with string escape seq: %s", cmd_buffer);
                                        
                                        // 将命令发送到解析队列
                                        static char *cmd_copy = NULL;
                                        // 释放之前的内存
                                        if (cmd_copy != NULL) {
                                            free(cmd_copy);
                                            cmd_copy = NULL;
                                        }
                                        // 分配内存并复制命令
                                        cmd_copy = (char *)malloc(cmd_index + 1);
                                        if (cmd_copy) {
                                            strcpy(cmd_copy, cmd_buffer);
                                            // 将命令送入队列
                                            if (uart_parser_send_command_to_queue(cmd_copy) != pdPASS) {
                                                ESP_LOGW(NETWORK_TASK_TAG, "Command queue is full, command discarded");
                                                free(cmd_copy);
                                                cmd_copy = NULL;
                                            }
                                        }
                                        
                                        // 重置命令缓冲区索引
                                        cmd_index = 0;
                                    }
                                } else {
                                    // 正常的反斜杠，添加到缓冲区
                                    if (cmd_index < sizeof(cmd_buffer) - 1) {
                                        cmd_buffer[cmd_index++] = c;
                                    }
                                }
                            } else {
                                // 将字符添加到命令缓冲区
                                if (cmd_index < sizeof(cmd_buffer) - 1) {
                                    cmd_buffer[cmd_index++] = c;
                                }
                            }
                        }
                    }
                } else {
                    // TCP客户端断开连接，更新连接状态标志，让网络监控任务处理重连
                    if (s_network_connected) {
                        ESP_LOGW(NETWORK_TASK_TAG, "TCP client disconnected, marked for reconnection");
                        s_network_connected = false;
                        
                        // 短暂延时，避免CPU占用
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }
                }
                break;
                
            case NETWORK_PROTOCOL_TCP_SERVER:
                if (s_tcp_server) {
                    WiFiClient client = s_tcp_server->available();
                    if (client) {
                        bytes_received = client.available();
                        if (bytes_received > 0) {
                            // 限制单次读取大小
                            int read_size = bytes_received > sizeof(rx_buffer) - 1 ? sizeof(rx_buffer) - 1 : bytes_received;
                            client.read((uint8_t*)rx_buffer, read_size);
                            rx_buffer[read_size] = '\0';
                            
                            ESP_LOGI(NETWORK_TASK_TAG, "TCP Server Received %d bytes: %s", read_size, rx_buffer);
                            
                            // 处理接收到的数据，转发给命令解析系统
                            // 与TCP客户端处理方式相同
                            for (int i = 0; i < read_size; i++) {
                                char c = rx_buffer[i];
                                
                                // 检测命令结束符 (回车或换行)
                                if (c == '\r' || c == '\n') {
                                    if (cmd_index > 0) {
                                        // 添加字符串结束符
                                        cmd_buffer[cmd_index] = '\0';
                                        
                                        ESP_LOGI(NETWORK_TASK_TAG, "Processing command from TCP Server: %s", cmd_buffer);
                                        
                                        // 将命令发送到解析队列
                                        static char *cmd_copy = NULL;
                                        // 释放之前的内存
                                        if (cmd_copy != NULL) {
                                            free(cmd_copy);
                                        }
                                        // 分配内存并复制命令
                                        cmd_copy = (char *)malloc(cmd_index + 1);
                                        if (cmd_copy) {
                                            strcpy(cmd_copy, cmd_buffer);
                                            // 将命令送入队列
                                            if (uart_parser_send_command_to_queue(cmd_copy) != pdPASS) {
                                                ESP_LOGW(NETWORK_TASK_TAG, "Command queue is full, command discarded");
                                                free(cmd_copy);
                                                cmd_copy = NULL;
                                            }
                                        }
                                        
                                        // 重置命令缓冲区索引
                                        cmd_index = 0;
                                    }
                                } else {
                                    // 将字符添加到命令缓冲区
                                    if (cmd_index < sizeof(cmd_buffer) - 1) {
                                        cmd_buffer[cmd_index++] = c;
                                    }
                                }
                            }
                        }
                    }
                }
                break;
                
            case NETWORK_PROTOCOL_UDP:
                if (s_udp) {
                    int packetSize = s_udp->parsePacket();
                    if (packetSize) {
                        // 限制单次读取大小
                        int read_size = packetSize > sizeof(rx_buffer) - 1 ? sizeof(rx_buffer) - 1 : packetSize;
                        s_udp->read((uint8_t*)rx_buffer, read_size);
                        rx_buffer[read_size] = '\0';
                        
                        ESP_LOGI(NETWORK_TASK_TAG, "UDP Received %d bytes from %s:%d: %s", 
                                read_size, s_udp->remoteIP().toString().c_str(), s_udp->remotePort(), rx_buffer);
                        
                        // 处理接收到的数据，转发给命令解析系统
                        // 与TCP客户端处理方式相同
                        for (int i = 0; i < read_size; i++) {
                            char c = rx_buffer[i];
                            
                            // 检测命令结束符 (回车或换行)
                            if (c == '\r' || c == '\n') {
                                if (cmd_index > 0) {
                                    // 添加字符串结束符
                                    cmd_buffer[cmd_index] = '\0';
                                    
                                    ESP_LOGI(NETWORK_TASK_TAG, "Processing command from UDP: %s", cmd_buffer);
                                    
                                    // 将命令发送到解析队列
                                    static char *cmd_copy = NULL;
                                    // 释放之前的内存
                                    if (cmd_copy != NULL) {
                                        free(cmd_copy);
                                    }
                                    // 分配内存并复制命令
                                    cmd_copy = (char *)malloc(cmd_index + 1);
                                    if (cmd_copy) {
                                        strcpy(cmd_copy, cmd_buffer);
                                        // 将命令送入队列
                                        if (uart_parser_send_command_to_queue(cmd_copy) != pdPASS) {
                                            ESP_LOGW(NETWORK_TASK_TAG, "Command queue is full, command discarded");
                                            free(cmd_copy);
                                            cmd_copy = NULL;
                                        }
                                    }
                                    
                                    // 重置命令缓冲区索引
                                    cmd_index = 0;
                                }
                            } else {
                                // 将字符添加到命令缓冲区
                                if (cmd_index < sizeof(cmd_buffer) - 1) {
                                    cmd_buffer[cmd_index++] = c;
                                }
                            }
                        }
                    }
                }
                break;
                
            default:
                break;
        }
        
        // 短暂延时，避免占用过多CPU
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
