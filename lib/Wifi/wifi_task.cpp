#include "wifi_task.h"
#include "WiFi.h"

#define WIFI_TASK_TAG "WIFI_TASK"
#define NETWORK_TASK_TAG "NETWORK_TASK"

static wifi_task_config_t* s_wifi_config = NULL;
static bool s_is_connected = false;
static bool s_network_connected = false;

// 网络对象
static WiFiClient* s_tcp_client = NULL;
static WiFiServer* s_tcp_server = NULL;
static WiFiUDP* s_udp = NULL;
static char s_network_info[256] = {0};

// 网络任务处理函数声明
static void network_task_handler(void *pvParameters);

static void wifi_task_handler(void *pvParameters)
{
    s_wifi_config = (wifi_task_config_t*)pvParameters;

    // A small delay to help prevent brownout if power supply is marginal
    vTaskDelay(pdMS_TO_TICKS(200));

    ESP_LOGI(WIFI_TASK_TAG, "Starting WiFi Task (Arduino)...");

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
            if (xTaskCreate(network_task_handler, "network_task", 4096, NULL, 4, NULL) != pdPASS) {
                ESP_LOGE(WIFI_TASK_TAG, "Failed to create network task");
            }
        }
    } else {
        s_is_connected = false;
        ESP_LOGW(WIFI_TASK_TAG, "WiFi connection failed or not in STA mode.");
    }

    // The task can now be deleted as WiFi connection is managed by the system
    vTaskDelete(NULL);
}

BaseType_t wifi_task_start(wifi_task_config_t *config)
{
    if (config == NULL) {
        return pdFAIL;
    }

    // Copy config to a static variable to be safely accessed by the task
    static wifi_task_config_t task_config;
    memcpy(&task_config, config, sizeof(wifi_task_config_t));

    return xTaskCreate(
        wifi_task_handler,
        "wifi_task_arduino",
        4096, // Stack size
        (void*)&task_config,
        5,    // Priority
        NULL
    );
}

bool is_wifi_connected(void)
{
    // For STA mode, we can rely on WiFi.isConnected()
    s_is_connected = WiFi.isConnected();
    return s_is_connected;
}

// 网络任务处理函数
static void network_task_handler(void *pvParameters)
{
    ESP_LOGI(NETWORK_TASK_TAG, "Starting network task...");
    
    network_config_t* net_config = &s_wifi_config->network_config;
    
    switch (net_config->protocol) {
        case NETWORK_PROTOCOL_TCP_CLIENT:
        {
            ESP_LOGI(NETWORK_TASK_TAG, "Initializing TCP Client mode");
            s_tcp_client = new WiFiClient();
            
            uint32_t start_time = millis();
            while (!s_tcp_client->connected()) {
                ESP_LOGI(NETWORK_TASK_TAG, "Connecting to TCP server %s:%d", 
                         net_config->remote_host, net_config->remote_port);
                
                if (s_tcp_client->connect(net_config->remote_host, net_config->remote_port)) {
                    s_network_connected = true;
                    ESP_LOGI(NETWORK_TASK_TAG, "TCP Client connected successfully");
                    snprintf(s_network_info, sizeof(s_network_info), 
                            "TCP Client connected to %s:%d", 
                            net_config->remote_host, net_config->remote_port);
                    break;
                } else {
                    ESP_LOGW(NETWORK_TASK_TAG, "TCP connection failed, retrying...");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                
                if (millis() - start_time > net_config->connect_timeout_ms) {
                    ESP_LOGE(NETWORK_TASK_TAG, "TCP connection timeout!");
                    break;
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
            } else {
                ESP_LOGE(NETWORK_TASK_TAG, "Failed to initialize UDP");
            }
            break;
        }
        
        default:
            ESP_LOGW(NETWORK_TASK_TAG, "Unknown network protocol");
            break;
    }
    
    // 网络任务完成初始化后删除自己
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
