#include "servo_commands.h"
#include "servo_controller.h"  // 使用新的简化控制器
#include "gripper_controller.h"  // 智能夹爪控制器
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

#define SERVO_CMD_TAG "SERVO_CMD"

void handle_servo_status(int argc, char *argv[]) {
    if (argc < 2) {
        ESP_LOGE(SERVO_CMD_TAG, "Usage: servo_status <servo_id>");
        return;
    }
    
    uint8_t servo_id = (uint8_t)atoi(argv[1]);
    servo_status_t status;
    
    if (servo_get_status(servo_id, &status)) {
        ESP_LOGI(SERVO_CMD_TAG, "=== Servo %d Status ===", servo_id);
        ESP_LOGI(SERVO_CMD_TAG, "Connected: %s", status.is_connected ? "Yes" : "No");
        ESP_LOGI(SERVO_CMD_TAG, "Work Mode: %s", 
                 status.work_mode == SERVO_MODE_SERVO ? "Servo" : "Motor");
        ESP_LOGI(SERVO_CMD_TAG, "Load State: %s", 
                 status.load_state == SERVO_LOAD_LOAD ? "Loaded" : "Unloaded");
        ESP_LOGI(SERVO_CMD_TAG, "Position: %.1f degrees", status.current_position);
        ESP_LOGI(SERVO_CMD_TAG, "Speed: %.1f", status.current_speed);
        ESP_LOGI(SERVO_CMD_TAG, "Temperature: %d°C", status.temperature);
        ESP_LOGI(SERVO_CMD_TAG, "Voltage: %.2fV", status.voltage);
        ESP_LOGI(SERVO_CMD_TAG, "Last Update: %lu ms", status.last_update_time);
        ESP_LOGI(SERVO_CMD_TAG, "==================");
    } else {
        ESP_LOGE(SERVO_CMD_TAG, "Failed to get status for servo %d", servo_id);
    }
}

void handle_servo_load(int argc, char *argv[]) {
    if (argc < 3) {
        ESP_LOGE(SERVO_CMD_TAG, "Usage: servo_load <servo_id> <0=unload|1=load>");
        return;
    }
    
    uint8_t servo_id = (uint8_t)atoi(argv[1]);
    int load_state_int = atoi(argv[2]);
    
    if (load_state_int != 0 && load_state_int != 1) {
        ESP_LOGE(SERVO_CMD_TAG, "Invalid load state: %d (use 0 for unload, 1 for load)", load_state_int);
        return;
    }
    
    servo_load_state_t load_state = (servo_load_state_t)load_state_int;
    
    if (servo_set_load_state(servo_id, load_state)) {
        ESP_LOGI(SERVO_CMD_TAG, "Successfully set servo %d to %s state", 
                 servo_id, load_state == SERVO_LOAD_LOAD ? "LOAD" : "UNLOAD");
    } else {
        ESP_LOGE(SERVO_CMD_TAG, "Failed to set load state for servo %d", servo_id);
    }
}

void handle_servo_mode(int argc, char *argv[]) {
    if (argc < 3) {
        ESP_LOGE(SERVO_CMD_TAG, "Usage: servo_mode <servo_id> <0=servo|1=motor>");
        return;
    }
    
    uint8_t servo_id = (uint8_t)atoi(argv[1]);
    int mode_int = atoi(argv[2]);
    
    if (mode_int != 0 && mode_int != 1) {
        ESP_LOGE(SERVO_CMD_TAG, "Invalid mode: %d (use 0 for servo, 1 for motor)", mode_int);
        return;
    }
    
    servo_mode_t mode = (servo_mode_t)mode_int;
    
    if (servo_set_work_mode(servo_id, mode)) {
        ESP_LOGI(SERVO_CMD_TAG, "Successfully set servo %d to %s mode", 
                 servo_id, mode == SERVO_MODE_SERVO ? "SERVO" : "MOTOR");
    } else {
        ESP_LOGE(SERVO_CMD_TAG, "Failed to set work mode for servo %d", servo_id);
    }
}

void handle_servo_position(int argc, char *argv[]) {
    if (argc < 4) {
        ESP_LOGE(SERVO_CMD_TAG, "Usage: servo_position <servo_id> <angle> <time_ms>");
        return;
    }
    
    uint8_t servo_id = (uint8_t)atoi(argv[1]);
    float angle = atof(argv[2]);
    uint32_t time_ms = (uint32_t)atoi(argv[3]);
    
    if (angle < 0 || angle > 240) {
        ESP_LOGE(SERVO_CMD_TAG, "Invalid angle: %.1f (valid range: 0-240)", angle);
        return;
    }
    
    if (time_ms < 20 || time_ms > 30000) {
        ESP_LOGE(SERVO_CMD_TAG, "Invalid time: %lu ms (valid range: 20-30000)", time_ms);
        return;
    }
    
    if (servo_control_position(servo_id, angle, time_ms)) {
        ESP_LOGI(SERVO_CMD_TAG, "Successfully commanded servo %d to move to %.1f° in %lu ms", 
                 servo_id, angle, time_ms);
    } else {
        ESP_LOGE(SERVO_CMD_TAG, "Failed to control position for servo %d", servo_id);
    }
}

void handle_servo_speed(int argc, char *argv[]) {
    if (argc < 3) {
        ESP_LOGE(SERVO_CMD_TAG, "Usage: servo_speed <servo_id> <speed>");
        return;
    }
    
    uint8_t servo_id = (uint8_t)atoi(argv[1]);
    int16_t speed = (int16_t)atoi(argv[2]);
    
    if (speed < -1000 || speed > 1000) {
        ESP_LOGE(SERVO_CMD_TAG, "Invalid speed: %d (valid range: -1000 to 1000)", speed);
        return;
    }
    
    if (servo_control_speed(servo_id, speed)) {
        ESP_LOGI(SERVO_CMD_TAG, "Successfully set servo %d motor speed to %d", servo_id, speed);
    } else {
        ESP_LOGE(SERVO_CMD_TAG, "Failed to control speed for servo %d", servo_id);
    }
}

void handle_servo_gripper(int argc, char *argv[]) {
    if (argc < 4) {
        ESP_LOGE(SERVO_CMD_TAG, "Usage: servo_gripper <servo_id> <percent> <time_ms>");
        ESP_LOGE(SERVO_CMD_TAG, "  percent: 0-100 (0=closed, 100=open)");
        ESP_LOGE(SERVO_CMD_TAG, "  time_ms: 20-30000");
        return;
    }
    
    uint8_t servo_id = (uint8_t)atoi(argv[1]);
    float gripper_percent = atof(argv[2]);
    uint32_t time_ms = (uint32_t)atoi(argv[3]);
    
    if (gripper_percent < 0 || gripper_percent > 100) {
        ESP_LOGE(SERVO_CMD_TAG, "Invalid gripper percent: %.1f (valid range: 0-100)", gripper_percent);
        return;
    }
    
    if (time_ms < 20 || time_ms > 30000) {
        ESP_LOGE(SERVO_CMD_TAG, "Invalid time: %lu ms (valid range: 20-30000)", time_ms);
        return;
    }
    
    if (servo_control_gripper(servo_id, gripper_percent, time_ms)) {
        ESP_LOGI(SERVO_CMD_TAG, "Successfully commanded gripper %d to %.1f%% in %lu ms", 
                 servo_id, gripper_percent, time_ms);
    } else {
        ESP_LOGE(SERVO_CMD_TAG, "Failed to control gripper for servo %d", servo_id);
    }
}

void handle_servo_gripper_config(int argc, char *argv[]) {
    if (argc < 5) {
        ESP_LOGE(SERVO_CMD_TAG, "Usage: servo_gripper_config <servo_id> <closed_angle> <open_angle> <min_step>");
        ESP_LOGE(SERVO_CMD_TAG, "  closed_angle: angle when gripper is closed (0-240)");
        ESP_LOGE(SERVO_CMD_TAG, "  open_angle: angle when gripper is open (0-240)");
        ESP_LOGE(SERVO_CMD_TAG, "  min_step: minimum step to overcome backlash (1-50)");
        return;
    }
    
    uint8_t servo_id = (uint8_t)atoi(argv[1]);
    float closed_angle = atof(argv[2]);
    float open_angle = atof(argv[3]);
    float min_step = atof(argv[4]);
    
    if (servo_configure_gripper_mapping(servo_id, closed_angle, open_angle, min_step)) {
        ESP_LOGI(SERVO_CMD_TAG, "Successfully configured gripper mapping for servo %d", servo_id);
        ESP_LOGI(SERVO_CMD_TAG, "  Closed: %.1f°, Open: %.1f°, MinStep: %.1f°", 
                 closed_angle, open_angle, min_step);
    } else {
        ESP_LOGE(SERVO_CMD_TAG, "Failed to configure gripper mapping for servo %d", servo_id);
    }
}

/* 智能夹爪控制命令实现 - 基于新的gripper_controller */

void handle_servo_gripper_smooth(int argc, char *argv[]) {
    if (argc < 3) {
        ESP_LOGE(SERVO_CMD_TAG, "Usage: servo_gripper_smooth <servo_id> <percent> [time_ms]");
        ESP_LOGE(SERVO_CMD_TAG, "  percent: 0-100 (0=closed, 100=open)");
        ESP_LOGE(SERVO_CMD_TAG, "  time_ms: optional execution time (auto if not specified)");
        return;
    }
    
    uint8_t servo_id = (uint8_t)atoi(argv[1]);
    float target_percent = atof(argv[2]);
    uint32_t time_ms = (argc >= 4) ? (uint32_t)atoi(argv[3]) : 0;  // 0 = auto time
    
    if (target_percent < 0 || target_percent > 100) {
        ESP_LOGE(SERVO_CMD_TAG, "Invalid gripper percent: %.1f (valid range: 0-100)", target_percent);
        return;
    }
    
    if (argc >= 4 && (time_ms < 100 || time_ms > 30000)) {
        ESP_LOGE(SERVO_CMD_TAG, "Invalid time: %lu ms (valid range: 100-30000)", time_ms);
        return;
    }
    
    if (gripper_control_smooth(servo_id, target_percent, time_ms)) {
        ESP_LOGI(SERVO_CMD_TAG, "Gripper %d smooth control started: target=%.1f%%, time=%lu ms", 
                 servo_id, target_percent, time_ms);
    } else {
        ESP_LOGE(SERVO_CMD_TAG, "Failed to start smooth control for gripper %d", servo_id);
    }
}

void handle_servo_gripper_status(int argc, char *argv[]) {
    if (argc < 2) {
        ESP_LOGE(SERVO_CMD_TAG, "Usage: servo_gripper_status <servo_id>");
        return;
    }
    
    uint8_t servo_id = (uint8_t)atoi(argv[1]);
    gripper_status_t status;
    
    if (gripper_get_status(servo_id, &status)) {
        ESP_LOGI(SERVO_CMD_TAG, "========== Gripper %d Status ==========", servo_id);
        ESP_LOGI(SERVO_CMD_TAG, "State: %s, Mode: %s", 
                 (status.state == GRIPPER_STATE_IDLE) ? "IDLE" :
                 (status.state == GRIPPER_STATE_MOVING) ? "MOVING" :
                 (status.state == GRIPPER_STATE_HOLDING) ? "HOLDING" :
                 (status.state == GRIPPER_STATE_ERROR) ? "ERROR" : 
                 (status.state == GRIPPER_STATE_CALIBRATING) ? "CALIBRATING" : "UNKNOWN",
                 (status.mode == GRIPPER_MODE_OPEN_LOOP) ? "OPEN_LOOP" :
                 (status.mode == GRIPPER_MODE_CLOSED_LOOP) ? "CLOSED_LOOP" :
                 (status.mode == GRIPPER_MODE_FORCE_CONTROL) ? "FORCE_CONTROL" : "UNKNOWN");
        
        ESP_LOGI(SERVO_CMD_TAG, "Position: %.1f%% (%.1f°), Target: %.1f%%", 
                 status.current_percent, status.current_angle, status.target_percent);
        
        ESP_LOGI(SERVO_CMD_TAG, "Moving: %s, Progress: %.1f%%", 
                 status.is_moving ? "YES" : "NO", status.movement_progress);
        
        ESP_LOGI(SERVO_CMD_TAG, "Feedback: %s, Position Error: %.2f%%", 
                 status.feedback_valid ? "VALID" : "INVALID", status.position_error);
        
        ESP_LOGI(SERVO_CMD_TAG, "Total Movements: %lu, Max Error: %.2f%%", 
                 status.total_movements, status.max_position_error);
        
        ESP_LOGI(SERVO_CMD_TAG, "Hardware Angle: %.1f°, Last Update: %lu ms ago", 
                 status.hardware_angle, 
                 (xTaskGetTickCount() * portTICK_PERIOD_MS) - status.last_update_time);
        ESP_LOGI(SERVO_CMD_TAG, "======================================");
    } else {
        ESP_LOGE(SERVO_CMD_TAG, "Failed to get status for gripper %d", servo_id);
    }
}

void handle_servo_gripper_mode(int argc, char *argv[]) {
    if (argc < 3) {
        ESP_LOGE(SERVO_CMD_TAG, "Usage: servo_gripper_mode <servo_id> <mode>");
        ESP_LOGE(SERVO_CMD_TAG, "  mode: open_loop | closed_loop | force_control");
        return;
    }
    
    uint8_t servo_id = (uint8_t)atoi(argv[1]);
    gripper_mode_e mode;
    
    if (strcmp(argv[2], "open_loop") == 0) {
        mode = GRIPPER_MODE_OPEN_LOOP;
    } else if (strcmp(argv[2], "closed_loop") == 0) {
        mode = GRIPPER_MODE_CLOSED_LOOP;
    } else if (strcmp(argv[2], "force_control") == 0) {
        mode = GRIPPER_MODE_FORCE_CONTROL;
    } else {
        ESP_LOGE(SERVO_CMD_TAG, "Invalid mode: %s", argv[2]);
        ESP_LOGE(SERVO_CMD_TAG, "Valid modes: open_loop, closed_loop, force_control");
        return;
    }
    
    if (gripper_set_mode(servo_id, mode)) {
        ESP_LOGI(SERVO_CMD_TAG, "Gripper %d mode set to: %s", servo_id, argv[2]);
    } else {
        ESP_LOGE(SERVO_CMD_TAG, "Failed to set mode for gripper %d", servo_id);
    }
}

void handle_servo_gripper_params(int argc, char *argv[]) {
    if (argc < 8) {
        ESP_LOGE(SERVO_CMD_TAG, "Usage: servo_gripper_params <servo_id> <slope_inc> <slope_dec> <pid_kp> <pid_ki> <pid_kd> <pid_limit>");
        ESP_LOGE(SERVO_CMD_TAG, "  slope_inc: slope increase rate (%%/cycle)");
        ESP_LOGE(SERVO_CMD_TAG, "  slope_dec: slope decrease rate (%%/cycle)");
        ESP_LOGE(SERVO_CMD_TAG, "  pid_kp: PID proportional gain");
        ESP_LOGE(SERVO_CMD_TAG, "  pid_ki: PID integral gain");
        ESP_LOGE(SERVO_CMD_TAG, "  pid_kd: PID derivative gain");
        ESP_LOGE(SERVO_CMD_TAG, "  pid_limit: PID output limit");
        return;
    }
    
    uint8_t servo_id = (uint8_t)atoi(argv[1]);
    
    gripper_control_params_t params = {
        .slope_increase_rate = atof(argv[2]),
        .slope_decrease_rate = atof(argv[3]),
        .slope_real_first = true,
        .pid_kp = atof(argv[4]),
        .pid_ki = atof(argv[5]),
        .pid_kd = atof(argv[6]),
        .pid_output_limit = atof(argv[7]),
        .pid_dead_zone = 0.5f,
        .static_friction_compensation = 2.0f,
        .dynamic_friction_coeff = 0.1f,
        .backlash_compensation = 1.0f,
        .max_position_error = 5.0f,
        .feedback_timeout_ms = 5000,
        .safety_stop_timeout = 30000
    };
    
    if (gripper_set_control_params(servo_id, &params)) {
        ESP_LOGI(SERVO_CMD_TAG, "Gripper %d control parameters updated:", servo_id);
        ESP_LOGI(SERVO_CMD_TAG, "  Slope: inc=%.2f, dec=%.2f", params.slope_increase_rate, params.slope_decrease_rate);
        ESP_LOGI(SERVO_CMD_TAG, "  PID: Kp=%.3f, Ki=%.3f, Kd=%.3f, Limit=%.1f", 
                 params.pid_kp, params.pid_ki, params.pid_kd, params.pid_output_limit);
    } else {
        ESP_LOGE(SERVO_CMD_TAG, "Failed to set control parameters for gripper %d", servo_id);
    }
}

void handle_servo_gripper_stop(int argc, char *argv[]) {
    if (argc < 2) {
        ESP_LOGE(SERVO_CMD_TAG, "Usage: servo_gripper_stop <servo_id>");
        return;
    }
    
    uint8_t servo_id = (uint8_t)atoi(argv[1]);
    
    if (gripper_stop(servo_id)) {
        ESP_LOGI(SERVO_CMD_TAG, "Gripper %d stopped successfully", servo_id);
    } else {
        ESP_LOGE(SERVO_CMD_TAG, "Failed to stop gripper %d", servo_id);
    }
}

void handle_servo_gripper_calibrate(int argc, char *argv[]) {
    if (argc < 3) {
        ESP_LOGE(SERVO_CMD_TAG, "Usage: servo_gripper_calibrate <servo_id> <position>");
        ESP_LOGE(SERVO_CMD_TAG, "  position: closed | open | <percent_value>");
        return;
    }
    
    uint8_t servo_id = (uint8_t)atoi(argv[1]);
    const char *position = argv[2];
    
    if (gripper_calibrate_position(servo_id, position)) {
        ESP_LOGI(SERVO_CMD_TAG, "Gripper %d calibrated at position: %s", servo_id, position);
    } else {
        ESP_LOGW(SERVO_CMD_TAG, "Calibration feature not yet implemented for gripper %d", servo_id);
    }
}

void handle_servo_gripper_test(int argc, char *argv[]) {
    if (argc < 5) {
        ESP_LOGE(SERVO_CMD_TAG, "Usage: servo_gripper_test <servo_id> <start_percent> <end_percent> <step_percent>");
        ESP_LOGE(SERVO_CMD_TAG, "  Example: servo_gripper_test 1 0 100 10");
        return;
    }
    
    uint8_t servo_id = (uint8_t)atoi(argv[1]);
    float start_percent = atof(argv[2]);
    float end_percent = atof(argv[3]);
    float step_percent = atof(argv[4]);
    
    if (start_percent < 0 || start_percent > 100 || end_percent < 0 || end_percent > 100) {
        ESP_LOGE(SERVO_CMD_TAG, "Invalid percent range: start=%.1f, end=%.1f", start_percent, end_percent);
        return;
    }
    
    if (step_percent <= 0 || step_percent > 50) {
        ESP_LOGE(SERVO_CMD_TAG, "Invalid step percent: %.1f (valid: 0.1-50)", step_percent);
        return;
    }
    
    if (gripper_precision_test(servo_id, start_percent, end_percent, step_percent)) {
        ESP_LOGI(SERVO_CMD_TAG, "Precision test started for gripper %d", servo_id);
    } else {
        ESP_LOGW(SERVO_CMD_TAG, "Precision test feature not yet implemented for gripper %d", servo_id);
    }
}
