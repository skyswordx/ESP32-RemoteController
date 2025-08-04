#ifndef WIFI_TASK_H
#define WIFI_TASK_H

#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "WiFi.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif // WIFI_TASK_H
