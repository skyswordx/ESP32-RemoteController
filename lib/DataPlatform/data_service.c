/**
 * @file data_service.c
 * @brief 數據服務層的實現
 *
 * @details
 * 此文件實現了 data_service.h 中聲明的所有功能。
 * 它管理一個靜態的全局系統狀態實例，並使用FreeRTOS的互斥鎖(Mutex)和
 * 事件標誌組(Event Group)來確保線程安全和高效的任務間通信。
 *
 * @note
 * 這個模塊是系統的核心，但它不包含任何與具體硬件平台相關的代碼，
 * 因此具有良好的可移植性。
 */

#include "data_service.h"
#include <string.h> // 用於 memcpy

/*============================================================================*/
/* 靜態全局變量 (Static Globals)                         */
/*============================================================================*/

/**
 * @brief 全局系統狀態的唯一實例
 * @details
 * 'static' 關鍵字確保此變量僅在該文件內可見，外部模塊必須通過API訪問。
 */
static system_state_t g_system_state;

/**
 * @brief 用於保護 g_system_state 的互斥鎖
 */
static SemaphoreHandle_t g_state_mutex = NULL;

/**
 * @brief 用於向其他任務發送實時通知的事件標誌組
 */
static EventGroupHandle_t g_system_events = NULL;


/*============================================================================*/
/* 公共API函數實現 (Public APIs)                         */
/*============================================================================*/

/**
 * @brief 初始化數據服務層
 */
BaseType_t data_service_init(void) {
    // 創建互斥鎖
    g_state_mutex = xSemaphoreCreateMutex();
    if (g_state_mutex == NULL) {
        // 錯誤處理：互斥鎖創建失敗
        return pdFAIL;
    }

    // 創建事件標誌組
    g_system_events = xEventGroupCreate();
    if (g_system_events == NULL) {
        // 錯誤處理：事件組創建失敗
        vSemaphoreDelete(g_state_mutex);
        return pdFAIL;
    }

    // 初始化系統狀態結構體為0或默認值
    memset(&g_system_state, 0, sizeof(system_state_t));

    return pdPASS;
}

/**
 * @brief 獲取整個系統狀態的線程安全副本（快照）
 */
void data_service_get_system_state(system_state_t *p_state_copy) {
    // 檢查輸入指針和互斥鎖是否有效
    if (p_state_copy == NULL || g_state_mutex == NULL) {
        return;
    }

    // 獲取互斥鎖，設置一個合理的等待超時
    if (xSemaphoreTake(g_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // 成功獲取鎖，複製數據
        memcpy(p_state_copy, &g_system_state, sizeof(system_state_t));
        
        // 釋放互斥鎖
        xSemaphoreGive(g_state_mutex);
    } else {
        // 錯誤處理：獲取鎖超時。
        memset(p_state_copy, 0, sizeof(system_state_t));
    }
}

/**
 * @brief 更新溫濕度數據
 */
void data_service_update_temp_humid(float temp, float humid) {
    if (g_state_mutex == NULL) return;

    if (xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        g_system_state.temperature = temp;
        g_system_state.humidity = humid;
        xSemaphoreGive(g_state_mutex);

        // 數據更新完成後，設置事件位通知其他任務
        if (g_system_events != NULL) {
            xEventGroupSetBits(g_system_events, BIT_EVENT_TEMP_HUMID_UPDATED);
        }
    }
}

/**
 * @brief 更新IMU數據
 */
void data_service_update_imu(const imu_data_t *p_imu_data) {
    if (p_imu_data == NULL || g_state_mutex == NULL) return;

    if (xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        memcpy(&g_system_state.imu_data, p_imu_data, sizeof(imu_data_t));
        xSemaphoreGive(g_state_mutex);

        if (g_system_events != NULL) {
            xEventGroupSetBits(g_system_events, BIT_EVENT_IMU_UPDATED);
        }
    }
}

/**
 * @brief 更新GPS數據
 */
void data_service_update_gps(const gps_data_t *p_gps_data) {
    if (p_gps_data == NULL || g_state_mutex == NULL) return;

    if (xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        memcpy(&g_system_state.gps_data, p_gps_data, sizeof(gps_data_t));
        xSemaphoreGive(g_state_mutex);

        if (g_system_events != NULL) {
            xEventGroupSetBits(g_system_events, BIT_EVENT_GPS_UPDATED);
        }
    }
}

/**
 * @brief 獲取系統事件標誌組的句柄
 */
EventGroupHandle_t data_service_get_event_group_handle(void) {
    return g_system_events;
}

/**
 * @brief 更新旋轉編碼器數據
 */
void data_service_update_encoder(const encoder_data_t *p_encoder_data) {
    if (p_encoder_data == NULL || g_state_mutex == NULL) return;

    if (xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        memcpy(&g_system_state.encoder_data, p_encoder_data, sizeof(encoder_data_t));
        xSemaphoreGive(g_state_mutex);

        if (g_system_events != NULL) {
            xEventGroupSetBits(g_system_events, BIT_EVENT_ENCODER_UPDATED);
        }
    }
}

/**
 * @brief 更新搖杆數據
 */
void data_service_update_joystick(const joystick_data_t *p_joystick_data) {
    if (p_joystick_data == NULL || g_state_mutex == NULL) return;

    if (xSemaphoreTake(g_state_mutex, portMAX_DELAY) == pdTRUE) {
        memcpy(&g_system_state.joystick_data, p_joystick_data, sizeof(joystick_data_t));
        xSemaphoreGive(g_state_mutex);

        if (g_system_events != NULL) {
            xEventGroupSetBits(g_system_events, BIT_EVENT_JOYSTICK_UPDATED);
        }
    }
}
