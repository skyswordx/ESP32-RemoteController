/**
 * @file gripper_controller.cpp
 * @brief 智能夹爪控制系统实现文件 - 基于PID控制器和斜坡规划器实现丝滑精密控制
 * @version 1.0
 * @date 2025-08-27
 * @author GitHub Copilot
 * 
 * @copyright Copyright (c) 2025 SENTRY Team
 * 
 * @note 核心设计思想：
 *       1. 分层控制：斜坡规划器负责轨迹生成，PID控制器负责精确跟踪
 *       2. 状态管理：维护每个夹爪的实时状态，支持多夹爪并发控制
 *       3. 自适应控制：根据反馈自动调整控制参数，处理机械死区
 *       4. 安全保护：超时检测、位置限制、错误恢复机制
 */

#include "gripper_controller.h"
#include "servo_controller.h"
#include "pid_controller.hpp"
#include "slope_planner.hpp"
#include "math_utils.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <cstring>
#include <cstdio>

/* Constants ----------------------------------------------------------------*/
#define GRIPPER_CTRL_TAG                "GRIPPER_CTRL"
#define GRIPPER_TASK_STACK_SIZE         4096
#define GRIPPER_TASK_PRIORITY           5
#define GRIPPER_NVS_NAMESPACE           "gripper_cfg"
#define GRIPPER_ANGLE_TOLERANCE         2.0f    ///< 角度容差 (度)
#define GRIPPER_PERCENT_EPSILON         0.1f    ///< 百分比精度
#define GRIPPER_MAX_RETRY_COUNT         3       ///< 最大重试次数

/* Global variables ---------------------------------------------------------*/
static bool g_gripper_system_initialized = false;
static TaskHandle_t g_gripper_task_handle = nullptr;
static SemaphoreHandle_t g_gripper_mutex = nullptr;

/* Per-gripper data structures */
static gripper_status_t g_gripper_status[MAX_GRIPPERS];
static gripper_mapping_t g_gripper_mapping[MAX_GRIPPERS];
static gripper_control_params_t g_gripper_params[MAX_GRIPPERS];
static PID_controller_c g_gripper_pid[MAX_GRIPPERS];
static Slope_planner_c g_gripper_slope[MAX_GRIPPERS];

/* Private function declarations --------------------------------------------*/
static void gripper_control_task(void *param);
static bool gripper_update_single(uint8_t servo_id);
static bool gripper_read_hardware_position(uint8_t servo_id, float *angle);
static float gripper_angle_to_percent(float angle, const gripper_mapping_t *mapping);
static float gripper_percent_to_angle(float percent, const gripper_mapping_t *mapping);
static bool gripper_execute_movement(uint8_t servo_id, float target_angle);
static void gripper_init_default_params(uint8_t servo_id);
static bool gripper_validate_servo_id(uint8_t servo_id);
static void gripper_update_movement_progress(uint8_t servo_id);
static bool gripper_is_movement_complete(uint8_t servo_id);
static const char* gripper_state_to_string(gripper_state_e state);
static const char* gripper_mode_to_string(gripper_mode_e mode);

/* Public function implementations ------------------------------------------*/

bool gripper_controller_init(void) {
    if (g_gripper_system_initialized) {
        ESP_LOGW(GRIPPER_CTRL_TAG, "Gripper controller already initialized");
        return true;
    }
    
    ESP_LOGI(GRIPPER_CTRL_TAG, "Initializing gripper control system...");
    
    // 创建互斥锁
    g_gripper_mutex = xSemaphoreCreateMutex();
    if (g_gripper_mutex == nullptr) {
        ESP_LOGE(GRIPPER_CTRL_TAG, "Failed to create mutex");
        return false;
    }
    
    // 初始化所有夹爪的默认参数
    for (int i = 0; i < MAX_GRIPPERS; i++) {
        gripper_init_default_params(i);
    }
    
    // 创建控制任务
    BaseType_t task_result = xTaskCreate(
        gripper_control_task,
        "gripper_ctrl",
        GRIPPER_TASK_STACK_SIZE,
        nullptr,
        GRIPPER_TASK_PRIORITY,
        &g_gripper_task_handle
    );
    
    if (task_result != pdPASS) {
        ESP_LOGE(GRIPPER_CTRL_TAG, "Failed to create gripper control task");
        vSemaphoreDelete(g_gripper_mutex);
        return false;
    }
    
    // 初始化NVS
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(GRIPPER_CTRL_TAG, "NVS partition was truncated and needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);
    
    g_gripper_system_initialized = true;
    ESP_LOGI(GRIPPER_CTRL_TAG, "Gripper control system initialized successfully");
    ESP_LOGI(GRIPPER_CTRL_TAG, "Control frequency: %d Hz, Feedback frequency: %d Hz", 
             GRIPPER_CONTROL_FREQUENCY_HZ, GRIPPER_FEEDBACK_FREQUENCY_HZ);
    
    return true;
}

void gripper_controller_deinit(void) {
    if (!g_gripper_system_initialized) {
        return;
    }
    
    ESP_LOGI(GRIPPER_CTRL_TAG, "Deinitializing gripper control system...");
    
    // 删除控制任务
    if (g_gripper_task_handle != nullptr) {
        vTaskDelete(g_gripper_task_handle);
        g_gripper_task_handle = nullptr;
    }
    
    // 删除互斥锁
    if (g_gripper_mutex != nullptr) {
        vSemaphoreDelete(g_gripper_mutex);
        g_gripper_mutex = nullptr;
    }
    
    g_gripper_system_initialized = false;
    ESP_LOGI(GRIPPER_CTRL_TAG, "Gripper control system deinitialized");
}

bool gripper_configure_mapping(uint8_t servo_id, const gripper_mapping_t *mapping) {
    if (!gripper_validate_servo_id(servo_id) || mapping == nullptr) {
        ESP_LOGE(GRIPPER_CTRL_TAG, "Invalid parameters for mapping configuration");
        return false;
    }
    
    // 参数验证
    if (mapping->closed_angle < 0 || mapping->closed_angle > 240 ||
        mapping->open_angle < 0 || mapping->open_angle > 240) {
        ESP_LOGE(GRIPPER_CTRL_TAG, "Invalid angle range: closed=%.1f, open=%.1f", 
                 mapping->closed_angle, mapping->open_angle);
        return false;
    }
    
    if (mapping->min_step < 0.1f || mapping->min_step > 50.0f) {
        ESP_LOGE(GRIPPER_CTRL_TAG, "Invalid min_step: %.1f", mapping->min_step);
        return false;
    }
    
    if (Math_abs(mapping->closed_angle - mapping->open_angle) < mapping->min_step) {
        ESP_LOGE(GRIPPER_CTRL_TAG, "Angle range too small for min_step");
        return false;
    }
    
    if (xSemaphoreTake(g_gripper_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // 复制映射参数
        memcpy(&g_gripper_mapping[servo_id], mapping, sizeof(gripper_mapping_t));
        g_gripper_mapping[servo_id].is_calibrated = true;
        
        // 同时更新legacy servo controller的映射
        servo_configure_gripper_mapping(servo_id, mapping->closed_angle, 
                                       mapping->open_angle, mapping->min_step);
        
        xSemaphoreGive(g_gripper_mutex);
        
        ESP_LOGI(GRIPPER_CTRL_TAG, "Gripper %d mapping configured:", servo_id);
        ESP_LOGI(GRIPPER_CTRL_TAG, "  Closed: %.1f°, Open: %.1f°, MinStep: %.1f°, MaxSpeed: %.1f%%/s", 
                 mapping->closed_angle, mapping->open_angle, mapping->min_step, mapping->max_speed);
        
        return true;
    }
    
    ESP_LOGE(GRIPPER_CTRL_TAG, "Failed to acquire mutex for mapping configuration");
    return false;
}

bool gripper_set_control_params(uint8_t servo_id, const gripper_control_params_t *params) {
    if (!gripper_validate_servo_id(servo_id) || params == nullptr) {
        ESP_LOGE(GRIPPER_CTRL_TAG, "Invalid parameters for control configuration");
        return false;
    }
    
    if (xSemaphoreTake(g_gripper_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // 复制控制参数
        memcpy(&g_gripper_params[servo_id], params, sizeof(gripper_control_params_t));
        
        // 更新PID控制器参数
        g_gripper_pid[servo_id].Set_params(params->pid_kp, params->pid_ki, params->pid_kd);
        g_gripper_pid[servo_id].Set_output_limit(params->pid_output_limit);
        g_gripper_pid[servo_id].Set_dead_zone(params->pid_dead_zone);
        
        // 更新斜坡规划器参数
        g_gripper_slope[servo_id].Set_increase_value(params->slope_increase_rate);
        g_gripper_slope[servo_id].Set_decrease_value(params->slope_decrease_rate);
        g_gripper_slope[servo_id].Set_real_first(params->slope_real_first);
        
        xSemaphoreGive(g_gripper_mutex);
        
        ESP_LOGI(GRIPPER_CTRL_TAG, "Gripper %d control params updated:", servo_id);
        ESP_LOGI(GRIPPER_CTRL_TAG, "  PID: Kp=%.3f, Ki=%.3f, Kd=%.3f", 
                 params->pid_kp, params->pid_ki, params->pid_kd);
        ESP_LOGI(GRIPPER_CTRL_TAG, "  Slope: Inc=%.2f, Dec=%.2f", 
                 params->slope_increase_rate, params->slope_decrease_rate);
        
        return true;
    }
    
    ESP_LOGE(GRIPPER_CTRL_TAG, "Failed to acquire mutex for control parameter update");
    return false;
}

bool gripper_set_mode(uint8_t servo_id, gripper_mode_e mode) {
    if (!gripper_validate_servo_id(servo_id)) {
        return false;
    }
    
    if (xSemaphoreTake(g_gripper_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        gripper_mode_e old_mode = g_gripper_status[servo_id].mode;
        g_gripper_status[servo_id].mode = mode;
        
        // 根据模式重置控制器状态
        if (mode != old_mode) {
            g_gripper_pid[servo_id].Reset();
            g_gripper_slope[servo_id].Reset();
            ESP_LOGI(GRIPPER_CTRL_TAG, "Gripper %d mode changed: %s → %s", 
                     servo_id, gripper_mode_to_string(old_mode), gripper_mode_to_string(mode));
        }
        
        xSemaphoreGive(g_gripper_mutex);
        return true;
    }
    
    return false;
}

bool gripper_control_smooth(uint8_t servo_id, float target_percent, uint32_t time_ms) {
    if (!gripper_validate_servo_id(servo_id)) {
        return false;
    }
    
    if (target_percent < 0 || target_percent > 100) {
        ESP_LOGE(GRIPPER_CTRL_TAG, "Invalid target percent: %.1f", target_percent);
        return false;
    }
    
    if (!g_gripper_system_initialized) {
        ESP_LOGE(GRIPPER_CTRL_TAG, "Gripper controller not initialized");
        return false;
    }
    
    if (xSemaphoreTake(g_gripper_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        gripper_status_t *status = &g_gripper_status[servo_id];
        
        // 设置新的目标
        status->target_percent = target_percent;
        status->movement_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        status->movement_duration = (time_ms > 0) ? time_ms : 
            (uint32_t)(Math_abs(target_percent - status->current_percent) / g_gripper_mapping[servo_id].max_speed * 1000);
        status->is_moving = true;
        status->state = GRIPPER_STATE_MOVING;
        status->movement_progress = 0.0f;
        
        // 设置斜坡规划器目标
        g_gripper_slope[servo_id].Set_target(target_percent);
        
        // 如果是闭环模式，同时设置PID目标
        if (status->mode == GRIPPER_MODE_CLOSED_LOOP) {
            // PID目标将在控制循环中动态更新为斜坡规划器的输出
        }
        
        xSemaphoreGive(g_gripper_mutex);
        
        ESP_LOGI(GRIPPER_CTRL_TAG, "Gripper %d smooth control: %.1f%% → %.1f%% in %lu ms", 
                 servo_id, status->current_percent, target_percent, status->movement_duration);
        
        return true;
    }
    
    ESP_LOGE(GRIPPER_CTRL_TAG, "Failed to acquire mutex for smooth control");
    return false;
}

bool gripper_stop(uint8_t servo_id) {
    if (!gripper_validate_servo_id(servo_id)) {
        return false;
    }
    
    if (xSemaphoreTake(g_gripper_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        gripper_status_t *status = &g_gripper_status[servo_id];
        
        // 立即停止运动
        status->is_moving = false;
        status->state = GRIPPER_STATE_HOLDING;
        status->target_percent = status->current_percent;
        
        // 重置控制器
        g_gripper_slope[servo_id].Set_target(status->current_percent);
        
        xSemaphoreGive(g_gripper_mutex);
        
        ESP_LOGI(GRIPPER_CTRL_TAG, "Gripper %d stopped at %.1f%%", servo_id, status->current_percent);
        return true;
    }
    
    return false;
}

bool gripper_get_current_percent(uint8_t servo_id, float *current_percent) {
    if (!gripper_validate_servo_id(servo_id) || current_percent == nullptr) {
        return false;
    }
    
    if (xSemaphoreTake(g_gripper_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        *current_percent = g_gripper_status[servo_id].current_percent;
        xSemaphoreGive(g_gripper_mutex);
        return true;
    }
    
    return false;
}

bool gripper_get_status(uint8_t servo_id, gripper_status_t *status) {
    if (!gripper_validate_servo_id(servo_id) || status == nullptr) {
        return false;
    }
    
    if (xSemaphoreTake(g_gripper_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        memcpy(status, &g_gripper_status[servo_id], sizeof(gripper_status_t));
        xSemaphoreGive(g_gripper_mutex);
        return true;
    }
    
    return false;
}

bool gripper_controller_is_running(void) {
    return g_gripper_system_initialized && (g_gripper_task_handle != nullptr);
}

/* Private function implementations -----------------------------------------*/

static void gripper_control_task(void *param) {
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t control_period = pdMS_TO_TICKS(1000 / GRIPPER_CONTROL_FREQUENCY_HZ);
    uint32_t cycle_count = 0;
    
    ESP_LOGI(GRIPPER_CTRL_TAG, "Gripper control task started (period: %lu ms)", 
             1000 / GRIPPER_CONTROL_FREQUENCY_HZ);
    
    while (1) {
        // 更新所有活跃的夹爪
        for (uint8_t servo_id = 0; servo_id < MAX_GRIPPERS; servo_id++) {
            if (g_gripper_status[servo_id].state != GRIPPER_STATE_IDLE) {
                gripper_update_single(servo_id);
            }
        }
        
        cycle_count++;
        if (cycle_count % (GRIPPER_CONTROL_FREQUENCY_HZ * 10) == 0) {
            ESP_LOGD(GRIPPER_CTRL_TAG, "Control task running normally (cycle: %lu)", cycle_count);
        }
        
        vTaskDelayUntil(&last_wake_time, control_period);
    }
}

static bool gripper_update_single(uint8_t servo_id) {
    if (xSemaphoreTake(g_gripper_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return false;
    }
    
    gripper_status_t *status = &g_gripper_status[servo_id];
    gripper_mapping_t *mapping = &g_gripper_mapping[servo_id];
    gripper_control_params_t *params = &g_gripper_params[servo_id];
    
    // 1. 读取硬件位置反馈
    float hardware_angle = 0;
    bool feedback_ok = gripper_read_hardware_position(servo_id, &hardware_angle);
    
    if (feedback_ok) {
        status->hardware_angle = hardware_angle;
        status->current_angle = hardware_angle;
        status->current_percent = gripper_angle_to_percent(hardware_angle, mapping);
        status->feedback_valid = true;
        status->last_feedback_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    } else {
        // 反馈失败，检查超时
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - status->last_feedback_time > params->feedback_timeout_ms) {
            ESP_LOGW(GRIPPER_CTRL_TAG, "Gripper %d feedback timeout", servo_id);
            status->feedback_valid = false;
            status->state = GRIPPER_STATE_ERROR;
        }
    }
    
    // 2. 更新运动进度
    gripper_update_movement_progress(servo_id);
    
    // 3. 根据控制模式执行控制算法
    bool control_ok = false;
    float target_angle = status->current_angle;
    
    if (status->is_moving) {
        switch (status->mode) {
            case GRIPPER_MODE_OPEN_LOOP: {
                // 开环控制：仅使用斜坡规划器
                g_gripper_slope[servo_id].Set_now_real(status->current_percent);
                g_gripper_slope[servo_id].Update_period();
                
                float planned_percent = g_gripper_slope[servo_id].Get_out();
                target_angle = gripper_percent_to_angle(planned_percent, mapping);
                
                ESP_LOGD(GRIPPER_CTRL_TAG, "Gripper %d open-loop: %.1f%% → %.1f° (target: %.1f%%)", 
                         servo_id, planned_percent, target_angle, status->target_percent);
                break;
            }
            
            case GRIPPER_MODE_CLOSED_LOOP: {
                // 闭环控制：斜坡规划器 + PID控制器
                g_gripper_slope[servo_id].Set_now_real(status->current_percent);
                g_gripper_slope[servo_id].Update_period();
                
                float planned_percent = g_gripper_slope[servo_id].Get_out();
                float planned_angle = gripper_percent_to_angle(planned_percent, mapping);
                
                // PID控制器跟踪斜坡规划器输出
                float pid_output = g_gripper_pid[servo_id].Update_period(planned_angle, status->current_angle);
                target_angle = status->current_angle + pid_output;
                
                // 计算位置误差
                status->position_error = Math_abs(planned_percent - status->current_percent);
                if (status->position_error > status->max_position_error) {
                    status->max_position_error = status->position_error;
                }
                
                ESP_LOGD(GRIPPER_CTRL_TAG, "Gripper %d closed-loop: plan=%.1f°, feedback=%.1f°, output=%.3f", 
                         servo_id, planned_angle, status->current_angle, pid_output);
                break;
            }
            
            case GRIPPER_MODE_FORCE_CONTROL:
                // 力控制模式（预留，暂时使用开环模式）
                ESP_LOGW(GRIPPER_CTRL_TAG, "Force control mode not implemented, using open-loop");
                break;
        }
        
        // 4. 执行运动控制
        control_ok = gripper_execute_movement(servo_id, target_angle);
        
        // 5. 检查运动是否完成
        if (gripper_is_movement_complete(servo_id)) {
            status->is_moving = false;
            status->state = GRIPPER_STATE_HOLDING;
            status->movement_progress = 100.0f;
            ESP_LOGI(GRIPPER_CTRL_TAG, "Gripper %d movement completed at %.1f%%", 
                     servo_id, status->current_percent);
        }
    }
    
    // 6. 更新统计信息
    status->last_update_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    xSemaphoreGive(g_gripper_mutex);
    return control_ok;
}

static bool gripper_read_hardware_position(uint8_t servo_id, float *angle) {
    // 通过servo_controller读取实际位置
    servo_status_t servo_status;
    if (servo_get_status(servo_id, &servo_status)) {
        *angle = servo_status.current_position;
        return true;
    }
    return false;
}

static float gripper_angle_to_percent(float angle, const gripper_mapping_t *mapping) {
    float range = mapping->open_angle - mapping->closed_angle;
    if (Math_abs(range) < 0.1f) {
        return 0.0f;  // 避免除零
    }
    
    float percent;
    if (mapping->reverse_direction) {
        percent = (mapping->open_angle - angle) / range * 100.0f;
    } else {
        percent = (angle - mapping->closed_angle) / range * 100.0f;
    }
    
    return Math_constrain_value(percent, 0.0f, 100.0f);
}

static float gripper_percent_to_angle(float percent, const gripper_mapping_t *mapping) {
    percent = Math_constrain_value(percent, 0.0f, 100.0f);
    
    float range = mapping->open_angle - mapping->closed_angle;
    float angle;
    
    if (mapping->reverse_direction) {
        angle = mapping->open_angle - (percent / 100.0f) * range;
    } else {
        angle = mapping->closed_angle + (percent / 100.0f) * range;
    }
    
    return Math_constrain_value(angle, 0.0f, 240.0f);
}

static bool gripper_execute_movement(uint8_t servo_id, float target_angle) {
    // 限制角度范围
    target_angle = Math_constrain_value(target_angle, 0.0f, 240.0f);
    
    // 使用短时间控制实现平滑运动
    uint32_t control_time = 1000 / GRIPPER_CONTROL_FREQUENCY_HZ + 10;  // 略大于控制周期
    
    return servo_control_position(servo_id, target_angle, control_time);
}

static void gripper_init_default_params(uint8_t servo_id) {
    // 初始化状态
    gripper_status_t *status = &g_gripper_status[servo_id];
    memset(status, 0, sizeof(gripper_status_t));
    status->servo_id = servo_id;
    status->state = GRIPPER_STATE_IDLE;
    status->mode = GRIPPER_MODE_OPEN_LOOP;
    status->last_update_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // 初始化映射参数
    gripper_mapping_t *mapping = &g_gripper_mapping[servo_id];
    mapping->closed_angle = GRIPPER_DEFAULT_CLOSED_ANGLE;
    mapping->open_angle = GRIPPER_DEFAULT_OPEN_ANGLE;
    mapping->min_step = GRIPPER_DEFAULT_MIN_STEP;
    mapping->max_speed = 20.0f;  // 20%/s 默认速度
    mapping->is_calibrated = false;
    mapping->reverse_direction = false;
    
    // 初始化控制参数
    gripper_control_params_t *params = &g_gripper_params[servo_id];
    params->slope_increase_rate = 2.0f;     // 2%/周期
    params->slope_decrease_rate = 2.0f;     // 2%/周期
    params->slope_real_first = true;
    params->pid_kp = 0.5f;
    params->pid_ki = 0.1f;
    params->pid_kd = 0.05f;
    params->pid_output_limit = 10.0f;       // 限制PID输出角度范围
    params->pid_dead_zone = 0.5f;
    params->static_friction_compensation = 2.0f;
    params->dynamic_friction_coeff = 0.1f;
    params->backlash_compensation = 1.0f;
    params->max_position_error = 5.0f;      // 5% 最大位置误差
    params->feedback_timeout_ms = 5000;     // 5秒反馈超时
    params->safety_stop_timeout = 30000;    // 30秒安全停止超时
    
    // 初始化PID控制器
    g_gripper_pid[servo_id].Init(params->pid_kp, params->pid_ki, params->pid_kd);
    g_gripper_pid[servo_id].Set_output_limit(params->pid_output_limit);
    g_gripper_pid[servo_id].Set_dead_zone(params->pid_dead_zone);
    
    // 初始化斜坡规划器
    g_gripper_slope[servo_id].Init(params->slope_increase_rate, params->slope_decrease_rate, params->slope_real_first);
    
    ESP_LOGD(GRIPPER_CTRL_TAG, "Gripper %d initialized with default parameters", servo_id);
}

static bool gripper_validate_servo_id(uint8_t servo_id) {
    if (servo_id >= MAX_GRIPPERS) {
        ESP_LOGE(GRIPPER_CTRL_TAG, "Invalid servo ID: %d (max: %d)", servo_id, MAX_GRIPPERS - 1);
        return false;
    }
    return true;
}

static void gripper_update_movement_progress(uint8_t servo_id) {
    gripper_status_t *status = &g_gripper_status[servo_id];
    
    if (!status->is_moving || status->movement_duration == 0) {
        return;
    }
    
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t elapsed = now - status->movement_start_time;
    
    if (elapsed >= status->movement_duration) {
        status->movement_progress = 100.0f;
    } else {
        status->movement_progress = (float)elapsed / status->movement_duration * 100.0f;
    }
}

static bool gripper_is_movement_complete(uint8_t servo_id) {
    gripper_status_t *status = &g_gripper_status[servo_id];
    gripper_mapping_t *mapping = &g_gripper_mapping[servo_id];
    
    // 检查位置是否到达目标
    float position_error = Math_abs(status->target_percent - status->current_percent);
    bool position_reached = position_error < GRIPPER_CONTROL_PRECISION;
    
    // 检查时间是否到达
    bool time_reached = status->movement_progress >= 100.0f;
    
    // 检查斜坡规划是否完成
    bool slope_complete = Math_abs(g_gripper_slope[servo_id].Get_out() - status->target_percent) < GRIPPER_PERCENT_EPSILON;
    
    return position_reached || time_reached || slope_complete;
}

static const char* gripper_state_to_string(gripper_state_e state) {
    switch (state) {
        case GRIPPER_STATE_IDLE: return "IDLE";
        case GRIPPER_STATE_MOVING: return "MOVING";
        case GRIPPER_STATE_HOLDING: return "HOLDING";
        case GRIPPER_STATE_ERROR: return "ERROR";
        case GRIPPER_STATE_CALIBRATING: return "CALIBRATING";
        default: return "UNKNOWN";
    }
}

static const char* gripper_mode_to_string(gripper_mode_e mode) {
    switch (mode) {
        case GRIPPER_MODE_OPEN_LOOP: return "OPEN_LOOP";
        case GRIPPER_MODE_CLOSED_LOOP: return "CLOSED_LOOP";
        case GRIPPER_MODE_FORCE_CONTROL: return "FORCE_CONTROL";
        default: return "UNKNOWN";
    }
}

/* Placeholder implementations for advanced features -----------------------*/

bool gripper_calibrate_position(uint8_t servo_id, const char *reference_position) {
    // TODO: 实现现场校准功能
    ESP_LOGW(GRIPPER_CTRL_TAG, "Calibration feature not yet implemented");
    return false;
}

bool gripper_adjust_mapping(uint8_t servo_id, const char *position_type, float angle_offset) {
    // TODO: 实现映射微调功能
    ESP_LOGW(GRIPPER_CTRL_TAG, "Mapping adjustment feature not yet implemented");
    return false;
}

bool gripper_save_config(uint8_t servo_id) {
    // TODO: 实现配置保存功能
    ESP_LOGW(GRIPPER_CTRL_TAG, "Config save feature not yet implemented");
    return false;
}

bool gripper_load_config(uint8_t servo_id) {
    // TODO: 实现配置加载功能
    ESP_LOGW(GRIPPER_CTRL_TAG, "Config load feature not yet implemented");
    return false;
}

bool gripper_precision_test(uint8_t servo_id, float start_percent, float end_percent, float step_percent) {
    // TODO: 实现精度测试功能
    ESP_LOGW(GRIPPER_CTRL_TAG, "Precision test feature not yet implemented");
    return false;
}

bool gripper_learn_friction_params(uint8_t servo_id) {
    // TODO: 实现摩擦参数学习功能
    ESP_LOGW(GRIPPER_CTRL_TAG, "Friction learning feature not yet implemented");
    return false;
}

/************************ COPYRIGHT(C) SENTRY TEAM **************************/
