/**
 * @file data_service.h
 * @brief 數據服務層公共頭文件
 *
 * @details
 * 該文件定義了數據服務層的公共接口、共享的系統狀態數據結構以及事件標誌。
 * 任何需要與系統核心數據交互的模塊都應包含此文件。
 * 該架構旨在實現高內聚、低耦合，並易於擴展和移植。
 *
 * @note
 * 為了在C++工程中兼容，使用了 extern "C" 關鍵字。
 */

#ifndef DATA_SERVICE_H
#define DATA_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "event_groups.h"
#include "semphr.h"

/*============================================================================*/
/* 事件標誌組定義 (Event Bits)                         */
/*============================================================================*/
/**
 * @brief 系統事件標誌位定義
 * 用於在數據更新時，向關心該數據的任務發送實時通知。
 */
#define BIT_EVENT_TEMP_HUMID_UPDATED   (1 << 0) // 溫濕度數據已更新
#define BIT_EVENT_IMU_UPDATED          (1 << 1) // IMU數據已更新
#define BIT_EVENT_GPS_UPDATED          (1 << 2) // GPS數據已更新
// 在此處為新的傳感器或事件添加更多的事件位...
// #define BIT_EVENT_NEW_SENSOR_UPDATED (1 << 3)

/*============================================================================*/
/* 系統狀態數據結構 (System State)                     */
/*============================================================================*/

// 下面是一些例子
/**
 * @brief IMU（慣性測量單元）數據結構
 */
typedef struct {
    float accel_x; // 加速度X軸
    float accel_y; // 加速度Y軸
    float accel_z; // 加速度Z軸
    float gyro_x;  // 角速度X軸
    float gyro_y;  // 角速度Y軸
    float gyro_z;  // 角速度Z軸
} imu_data_t;

/**
 * @brief GPS數據結構
 */
typedef struct {
    double latitude;  // 緯度
    double longitude; // 經度
    float  speed;     // 速度
    uint8_t satellites_in_view; // 可視衛星數
} gps_data_t;

/**
 * @brief 系統狀態緩存的完整數據結構
 * @details
 * 這是系統中所有共享數據的集合。數據服務層的核心職責就是維護該結構的
 * 一個全局實例，並保證對其訪問的線程安全。
 */
typedef struct {
    float      temperature; // 溫度 (°C)
    float      humidity;    // 濕度 (%)
    imu_data_t imu_data;    // IMU數據
    gps_data_t gps_data;    // GPS數據
    // 在此處為新的傳感器添加數據字段...
    // uint32_t new_sensor_value;
} system_state_t;


/*============================================================================*/
/* 公共API函數聲明 (Public APIs)                         */
/*============================================================================*/

/**
 * @brief 初始化數據服務層
 * @details
 * 必須在任何其他API被調用之前，在系統啟動時調用一次。
 * 此函數會創建所需的Mutex和Event Group。
 * @return pdPASS 初始化成功, pdFAIL 初始化失敗.
 */
BaseType_t data_service_init(void);

/**
 * @brief 獲取整個系統狀態的線程安全副本（快照）
 * @details
 * 此函數會鎖定互斥鎖，將當前的全局系統狀態複製到用戶提供的指針中，然後解鎖。
 * 這是應用層任務獲取數據的主要方式。
 * @param[out] p_state_copy 指向用戶提供的 system_state_t 結構體，用於存儲狀態副本。
 */
void data_service_get_system_state(system_state_t *p_state_copy);

/**
 * @brief 更新溫濕度數據
 * @details
 * 由溫濕度傳感器任務調用。此函數會更新全局狀態並設置相應的事件位。
 * @param[in] temp 最新溫度值
 * @param[in] humid 最新濕度值
 */
void data_service_update_temp_humid(float temp, float humid);

/**
 * @brief 更新IMU數據
 * @details
 * 由IMU傳感器任務調用。
 * @param[in] p_imu_data 指向包含最新IMU數據的結構體。
 */
void data_service_update_imu(const imu_data_t *p_imu_data);

/**
 * @brief 更新GPS數據
 * @details
 * 由GPS傳感器任務調用。
 * @param[in] p_gps_data 指向包含最新GPS數據的結構體。
 */
void data_service_update_gps(const gps_data_t *p_gps_data);

// 在此處為新的傳感器添加更新函數的聲明...
// void data_service_update_new_sensor(uint32_t value);


/**
 * @brief 獲取系統事件標誌組的句柄
 * @details
 * 允許需要等待特定事件的任務（如報警任務）獲取Event Group句柄，
 * 以便使用 xEventGroupWaitBits() 函數。
 * @return EventGroupHandle_t 事件標誌組的句柄，如果未初始化則返回NULL。
 */
EventGroupHandle_t data_service_get_event_group_handle(void);


#ifdef __cplusplus
}
#endif

#endif // DATA_SERVICE_H
