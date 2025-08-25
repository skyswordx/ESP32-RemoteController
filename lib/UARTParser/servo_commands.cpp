#include "servo_commands.h"
#include "servo_controller.h"  // 使用新的简化控制器
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
