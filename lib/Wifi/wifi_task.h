#ifndef WIFI_TASK_H
#define WIFI_TASK_H

#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "WiFi.h"
#include "WiFiClient.h"
#include "WiFiServer.h"
#include "WiFiUdp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 网络协议类型
 */
typedef enum {
    NETWORK_PROTOCOL_NONE = 0,      /*!< 不使用网络协议 */
    NETWORK_PROTOCOL_TCP_CLIENT,    /*!< TCP 客户端模式 */
    NETWORK_PROTOCOL_TCP_SERVER,    /*!< TCP 服务端模式 */
    NETWORK_PROTOCOL_UDP            /*!< UDP 模式 */
} network_protocol_t;

/**
 * @brief TCP/UDP 配置结构体
 */
typedef struct {
    network_protocol_t protocol;    /*!< 网络协议类型 */
    char remote_host[64];           /*!< 远程主机 IP 地址或域名 (客户端模式使用) */
    uint16_t remote_port;           /*!< 远程端口 (客户端模式使用) */
    uint16_t local_port;            /*!< 本地端口 (服务端模式或 UDP 使用) */
    bool auto_connect;              /*!< WiFi 连接成功后是否自动开始网络连接 */
    uint32_t connect_timeout_ms;    /*!< 连接超时时间 (ms) */
} network_config_t;

/**
 * @brief WiFi 任务配置结构体 (基于 Arduino)
 */
typedef struct {
    wifi_mode_t wifi_mode;              /*!< WiFi 模式 (WIFI_STA, WIFI_AP, WIFI_AP_STA) */
    char ssid[33];                      /*!< WiFi SSID */
    char password[65];                  /*!< WiFi 密码 */
    char ap_ssid[33];                   /*!< AP 模式下的 SSID */
    char ap_password[65];               /*!< AP 模式下的密码 */
    bool power_save;                    /*!< 是否开启 WiFi 省电模式 */
    wifi_power_t tx_power;              /*!< WiFi 发射功率 */
    uint32_t sta_connect_timeout_ms;    /*!< STA 模式下连接超时时间 (ms) */
    network_config_t network_config;    /*!< 网络协议配置 */
} wifi_task_config_t;

/**
 * @brief 启动 WiFi 任务
 *
 * @param config 指向 wifi_task_config_t 结构体的指针，包含 WiFi 任务的配置信息
 * @return 
 *      - pdPASS: 任务创建成功
 *      - 其他: 任务创建失败
 */
BaseType_t wifi_task_start(wifi_task_config_t *config);

/**
 * @brief 获取 WiFi 连接状态 (仅在 STA 模式下有效)
 *
 * @return 
 *      - true: 已连接到 AP
 *      - false: 未连接到 AP
 */
bool is_wifi_connected(void);

/**
 * @brief 发送数据 (TCP 或 UDP)
 *
 * @param data 要发送的数据
 * @param len 数据长度
 * @return 
 *      - 发送的字节数: 成功
 *      - -1: 失败
 */
int network_send_data(const uint8_t* data, size_t len);

/**
 * @brief 发送字符串 (TCP 或 UDP)
 *
 * @param str 要发送的字符串
 * @return 
 *      - 发送的字节数: 成功
 *      - -1: 失败
 */
int network_send_string(const char* str);

/**
 * @brief 检查网络连接状态
 *
 * @return 
 *      - true: 网络连接正常
 *      - false: 网络连接断开
 */
bool is_network_connected(void);

/**
 * @brief 获取网络连接信息
 *
 * @return 网络连接信息字符串
 */
const char* get_network_info(void);

/**
 * @brief 断开当前 WiFi 连接
 *
 * @return 
 *      - true: 成功断开
 *      - false: 断开失败
 */
bool wifi_disconnect(void);

/**
 * @brief 使用新的 SSID 和密码连接 WiFi
 *
 * @param ssid 新的 WiFi SSID
 * @param password 新的 WiFi 密码
 * @param timeout_ms 连接超时时间 (ms)
 * @return 
 *      - true: 连接成功
 *      - false: 连接失败
 */
bool wifi_connect_new(const char* ssid, const char* password, uint32_t timeout_ms);

/**
 * @brief 获取当前 WiFi 配置信息
 *
 * @param config 用于存储当前配置的结构体指针
 * @return 
 *      - true: 获取成功
 *      - false: 获取失败
 */
bool get_current_wifi_config(wifi_task_config_t* config);

/**
 * @brief 断开当前网络连接 (TCP/UDP)
 *
 * @return 
 *      - true: 成功断开
 *      - false: 断开失败
 */
bool network_disconnect(void);

/**
 * @brief 配置 TCP 客户端参数并连接
 *
 * @param remote_host 远程主机 IP 地址
 * @param remote_port 远程主机端口
 * @param timeout_ms 连接超时时间 (ms)
 * @return 
 *      - true: 连接成功
 *      - false: 连接失败
 */
bool network_connect_tcp_client(const char* remote_host, uint16_t remote_port, uint32_t timeout_ms);

/**
 * @brief 获取当前网络配置信息
 *
 * @param config 用于存储当前网络配置的结构体指针
 * @return 
 *      - true: 获取成功
 *      - false: 获取失败
 */
bool get_current_network_config(network_config_t* config);

#ifdef __cplusplus
}
#endif

#endif // WIFI_TASK_H
