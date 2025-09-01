#include "uart_parser.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* 包含FreeRTOS头文件 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
// 包含 ESP-IDF 和项目相关的头文件
#include "esp_system.h"
#include "esp_log.h"
#include "soc/soc.h"  // 包含 CPU 频率相关函数
#include "wifi_task.h" // 包含用于获取WiFi状态的函数
#include "Arduino.h" // 包含 Arduino 功能，如 WiFi.localIP()

/* 宏定义 */
#define UART_PARSER_QUEUE_LENGTH    8      // 命令队列深度
#define UART_PARSER_MAX_CMD_LEN     64     // 支持的最大命令长度
#define UART_PARSER_MAX_ARGS        8      // 支持的最大参数数量

/* FreeRTOS 相关的句柄 */
static QueueHandle_t uart_command_queue = NULL; // 用于接收命令字符串指针的消息队列


/* -------------------- 1. 命令处理函数的实现 -------------------- */
#include "serial_servo.h"

// 串口舵机对象，使用硬件串口2（可根据实际硬件修改）
static HardwareSerial SerialServoUart(2);
static SerialServo serialServo(SerialServoUart);
static bool serialServoInited = false;

static void ensure_serial_servo_inited() {
    if (!serialServoInited) {
        serialServo.begin(115200); // 默认波特率，可根据实际情况修改
        serialServoInited = true;
    }
}

static void handle_servo_status(int argc, char *argv[]);
static void handle_servo_load(int argc, char *argv[]);
static void handle_servo_mode(int argc, char *argv[]);
static void handle_servo_position(int argc, char *argv[]);
static void handle_servo_speed(int argc, char *argv[]);

/* 新增舵机命令处理函数声明 */
static void handle_servo_get_cmd_position(int argc, char *argv[]);
static void handle_servo_read_now_position(int argc, char *argv[]);
static void handle_servo_position_delay(int argc, char *argv[]);
static void handle_servo_position_test(int argc, char *argv[]);
static void handle_servo_get_delay(int argc, char *argv[]);
static void handle_servo_offset(int argc, char *argv[]);
static void handle_servo_get_offset(int argc, char *argv[]);
static void handle_servo_angle_range(int argc, char *argv[]);
static void handle_servo_get_angle_range(int argc, char *argv[]);
static void handle_servo_voltage_range(int argc, char *argv[]);

/**
 * @brief 'help' 命令的处理函数。
 * 它会遍历命令表并打印所有命令的帮助信息。
 */
static void handle_help(int argc, char *argv[]);

/**
 * @brief 'reboot' 命令的处理函数。
 */
static void handle_reboot(int argc, char *argv[]);

/**
 * @brief 'get_sys_info' 命令的处理函数。
 * 用法: get_sys_info
 */
static void handle_get_sys_info(int argc, char *argv[]);

/**
 * @brief 'get_wifi_status' 命令的处理函数。
 * 用法: get_wifi_status
 */
static void handle_get_wifi_status(int argc, char *argv[]);

/**
 * @brief 'wifi_disconnect' 命令的处理函数。
 * 用法: wifi_disconnect
 */
static void handle_wifi_disconnect(int argc, char *argv[]);

/**
 * @brief 'wifi_connect' 命令的处理函数。
 * 用法: wifi_connect <ssid> [password]
 */
static void handle_wifi_connect(int argc, char *argv[]);

/**
 * @brief 'wifi_config' 命令的处理函数。
 * 用法: wifi_config
 */
static void handle_wifi_config(int argc, char *argv[]);

/**
 * @brief 'network_status' 命令的处理函数。
 * 用法: network_status
 */
static void handle_network_status(int argc, char *argv[]);

/**
 * @brief 'network_disconnect' 命令的处理函数。
 * 用法: network_disconnect
 */
static void handle_network_disconnect(int argc, char *argv[]);

/**
 * @brief 'tcp_connect' 命令的处理函数。
 * 用法: tcp_connect <host> <port>
 */
static void handle_tcp_connect(int argc, char *argv[]);

/**
 * @brief 'network_config' 命令的处理函数。
 * 用法: network_config
 */
static void handle_network_config(int argc, char *argv[]);

/**
 * @brief 'network_send' 命令的处理函数。
 * 用法: network_send <message>
 */
static void handle_network_send(int argc, char *argv[]);

/**
 * @brief 'wifi_reconnect' 命令的处理函数。
 * 用法: wifi_reconnect
 */
static void handle_wifi_reconnect(int argc, char *argv[]);

/**
 * @brief 'network_reconnect' 命令的处理函数。
 * 用法: network_reconnect
 */
static void handle_network_reconnect(int argc, char *argv[]);


/* -------------------- 2. 命令分派表 -------------------- */
// 在这里将您的命令和处理函数关联起来。

static const command_t command_table[] = {
    /* 命令名           处理函数指针              帮助信息字符串 */
    {"help",            handle_help,            "help: 显示所有可用命令。"},
    {"reboot",          handle_reboot,          "reboot: 重启设备。"},
    {"get_sys_info",    handle_get_sys_info,    "get_sys_info: 获取系统信息。"},
    {"get_wifi_status", handle_get_wifi_status, "get_wifi_status: 获取WiFi连接状态。"},
    
    /* WiFi 控制命令 */
    {"wifi_disconnect", handle_wifi_disconnect, "wifi_disconnect: 断开当前WiFi连接。"},
    {"wifi_connect",    handle_wifi_connect,    "wifi_connect <ssid> [password]: 连接到指定WiFi网络。"},
    {"wifi_config",     handle_wifi_config,     "wifi_config: 显示当前WiFi配置信息。"},
    {"wifi_reconnect",  handle_wifi_reconnect,  "wifi_reconnect: 使用当前配置重新连接WiFi。"},
    
    /* 网络协议控制命令 */
    {"network_status",     handle_network_status,     "network_status: 获取当前网络协议状态。"},
    {"network_disconnect", handle_network_disconnect, "network_disconnect: 断开当前网络连接。"},
    {"tcp_connect",        handle_tcp_connect,        "tcp_connect <host> <port>: 连接到TCP服务器。"},
    {"network_config",     handle_network_config,     "network_config: 显示当前网络配置信息。"},
    {"network_send",       handle_network_send,       "network_send <message>: 通过网络发送消息。"},
    {"network_reconnect",  handle_network_reconnect,  "network_reconnect: 使用当前配置重新连接网络。"},
    
    /* 舵机控制命令 */
    {"servo_status",       handle_servo_status,       "servo_status <servo_id>: 查询指定舵机角度/温度/电压。"},
    {"servo_load",         handle_servo_load,         "servo_load <servo_id> <0|1>: 设置舵机负载(1=加载,0=卸载)。"},
    {"servo_mode",         handle_servo_mode,         "servo_mode <servo_id> <0|1>: 设置舵机模式(0=舵机,1=电机)。"},
    {"servo_position",     handle_servo_position,     "servo_position <servo_id> <angle> <time_ms>: 控制舵机转到角度。"},
    {"servo_speed",        handle_servo_speed,        "servo_speed <servo_id> <speed>: 电机模式下设置速度。"},
    
    /* 新增舵机命令 */
    {"servo_get_cmd_position", handle_servo_get_cmd_position, "servo_get_cmd_position <servo_id>: 获取舵机的当前预设位置和时间。"},
    {"servo_read_now_position", handle_servo_read_now_position, "servo_read_now_position <servo_id>: 读取舵机的实时当前角度位置。"},
    {"servo_position_delay", handle_servo_position_delay, "servo_position_delay <servo_id> <angle> <time_ms>: 设置延时执行舵机运动。"},
    {"servo_position_test", handle_servo_position_test, "servo_position_test <servo_id> <angle> <time_ms>: 测试舵机运动并记录预设值与实际值。"},
    {"servo_get_delay",    handle_servo_get_delay,    "servo_get_delay <servo_id>: 获取舵机延时执行的预设位置。"},
    {"servo_offset",       handle_servo_offset,       "servo_offset <servo_id> <angle> <save>: 设置舵机角度偏移(save=0|1)。"},
    {"servo_get_offset",   handle_servo_get_offset,   "servo_get_offset <servo_id>: 获取舵机角度偏移值。"},
    {"servo_angle_range",  handle_servo_angle_range,  "servo_angle_range <servo_id> <min> <max>: 设置舵机角度范围限制。"},
    {"servo_get_range",    handle_servo_get_angle_range, "servo_get_range <servo_id>: 获取舵机角度范围限制。"},
    {"servo_voltage_range",handle_servo_voltage_range,"servo_voltage_range <servo_id> <min> <max>: 设置舵机电压范围限制。"},
    /* --- 您可以在此行下方添加您的新命令 --- */
};

static void handle_servo_status(int argc, char *argv[])
{
    if (argc < 2) {
        uart_parser_put_string("用法: servo_status <servo_id>\r\n");
        return;
    }
    ensure_serial_servo_inited();
    uint8_t id = (uint8_t)atoi(argv[1]);
    float angle = 0;
    int temp = 0;
    float voltage = 0;
    char buf[128];
    if (serialServo.read_servo_position(id, angle) == Operation_Success &&
        serialServo.read_servo_temp(id, temp) == Operation_Success &&
        serialServo.read_servo_voltage(id, voltage) == Operation_Success) {
        snprintf(buf, sizeof(buf), "Servo %d 状态: 角度=%.2f°, 温度=%d°C, 电压=%.2fV\r\n", id, angle, temp, voltage);
        uart_parser_put_string(buf);
    } else {
        uart_parser_put_string("读取舵机状态失败\r\n");
    }
}

static void handle_servo_load(int argc, char *argv[])
{
    if (argc < 3) {
        uart_parser_put_string("用法: servo_load <servo_id> <0|1>\r\n");
        return;
    }
    ensure_serial_servo_inited();
    uint8_t id = (uint8_t)atoi(argv[1]);
    int load = atoi(argv[2]);
    if (serialServo.set_servo_motor_load(id, load ? false : true) == Operation_Success) {
        uart_parser_put_string(load ? "舵机已加载\r\n" : "舵机已卸载\r\n");
    } else {
        uart_parser_put_string("设置舵机负载失败\r\n");
    }
}

static void handle_servo_mode(int argc, char *argv[])
{
    if (argc < 3) {
        uart_parser_put_string("用法: servo_mode <servo_id> <0|1>\r\n");
        return;
    }
    ensure_serial_servo_inited();
    uint8_t id = (uint8_t)atoi(argv[1]);
    int mode = atoi(argv[2]);
    int speed = 0;
    // 先读当前速度
    serialServo.get_servo_mode_and_speed(id, mode, speed);
    if (serialServo.set_servo_mode_and_speed(id, mode, speed) == Operation_Success) {
        uart_parser_put_string(mode ? "已切换为电机模式\r\n" : "已切换为舵机模式\r\n");
    } else {
        uart_parser_put_string("设置舵机模式失败\r\n");
    }
}

static void handle_servo_position(int argc, char *argv[])
{
    if (argc < 4) {
        uart_parser_put_string("用法: servo_position <servo_id> <angle> <time_ms>\r\n");
        return;
    }
    ensure_serial_servo_inited();
    uint8_t id = (uint8_t)atoi(argv[1]);
    float angle = (float)atof(argv[2]);
    uint16_t time_ms = (uint16_t)atoi(argv[3]);
    if (serialServo.move_servo_immediate(id, angle, time_ms) == Operation_Success) {
        uart_parser_put_string("舵机移动指令已发送\r\n");
    } else {
        uart_parser_put_string("舵机移动失败\r\n");
    }
}

static void handle_servo_speed(int argc, char *argv[])
{
    if (argc < 3) {
        uart_parser_put_string("用法: servo_speed <servo_id> <speed>\r\n");
        return;
    }
    ensure_serial_servo_inited();
    uint8_t id = (uint8_t)atoi(argv[1]);
    int speed = atoi(argv[2]);
    // 先读当前模式
    int mode = 1;
    serialServo.get_servo_mode_and_speed(id, mode, speed);
    if (serialServo.set_servo_mode_and_speed(id, 1, speed) == Operation_Success) {
        uart_parser_put_string("电机速度设置成功\r\n");
    } else {
        uart_parser_put_string("设置电机速度失败\r\n");
    }
}

/* ------------- 新增舵机命令实现 ------------- */

static void handle_servo_get_cmd_position(int argc, char *argv[])
{
    if (argc < 2) {
        uart_parser_put_string("用法: servo_get_cmd_position <servo_id>\r\n");
        return;
    }
    ensure_serial_servo_inited();
    uint8_t id = (uint8_t)atoi(argv[1]);
    float angle = 0;
    uint16_t time_ms = 0;
    char buf[128];
    
    if (serialServo.get_servo_move_immediate(id, &angle, &time_ms) == Operation_Success) {
        snprintf(buf, sizeof(buf), "Servo %d 预设位置: 角度=%.2f°, 执行时间=%d毫秒\r\n", id, angle, time_ms);
        uart_parser_put_string(buf);
    } else {
        uart_parser_put_string("获取舵机预设位置失败\r\n");
    }
}

static void handle_servo_read_now_position(int argc, char *argv[])
{
    if (argc < 2) {
        uart_parser_put_string("用法: servo_read_now_position <servo_id>\r\n");
        return;
    }
    ensure_serial_servo_inited();
    uint8_t id = (uint8_t)atoi(argv[1]);
    float position = 0;
    char buf[128];
    
    if (serialServo.read_servo_position(id, position) == Operation_Success) {
        snprintf(buf, sizeof(buf), "Servo %d 实时位置: 角度=%.2f°\r\n", id, position);
        uart_parser_put_string(buf);
    } else {
        uart_parser_put_string("读取舵机实时位置失败\r\n");
    }
}

static void handle_servo_position_test(int argc, char *argv[])
{
    if (argc < 4) {
        uart_parser_put_string("用法: servo_position_test <servo_id> <angle> <time_ms>\r\n");
        return;
    }
    ensure_serial_servo_inited();
    uint8_t id = (uint8_t)atoi(argv[1]);
    float target_angle = (float)atof(argv[2]);
    uint16_t time_ms = (uint16_t)atoi(argv[3]);
    
    // 变量用于存储预设值和实际值
    float preset_angle = 0;
    uint16_t preset_time = 0;
    float actual_angle = 0;
    char buf[256];
    
    // 首先移动舵机
    if (serialServo.move_servo_immediate(id, target_angle, time_ms) != Operation_Success) {
        uart_parser_put_string("舵机移动指令发送失败\r\n");
        return;
    }
    
    // 获取预设值（应该和我们设置的一样，但为确保准确性，从舵机读取）
    if (serialServo.get_servo_move_immediate(id, &preset_angle, &preset_time) != Operation_Success) {
        uart_parser_put_string("获取舵机预设位置失败\r\n");
        return;
    }
    
    // 输出初始信息
    snprintf(buf, sizeof(buf), "舵机测试开始: ID=%d, 目标角度=%.2f°, 执行时间=%d毫秒\r\n", 
             id, target_angle, time_ms);
    uart_parser_put_string(buf);
    
    // 等待运动完成（等待时间稍微长于设定的运动时间）
    snprintf(buf, sizeof(buf), "等待舵机运动完成 (%d毫秒)...\r\n", time_ms);
    uart_parser_put_string(buf);
    vTaskDelay(pdMS_TO_TICKS(time_ms + 100));
    
    // 读取舵机实际达到的位置
    if (serialServo.read_servo_position(id, actual_angle) != Operation_Success) {
        uart_parser_put_string("读取舵机实际位置失败\r\n");
        return;
    }
    
    // 计算误差
    float input_error = actual_angle - target_angle;
    float preset_error = actual_angle - preset_angle;

    // 格式化输出结果
    snprintf(buf, sizeof(buf), 
             "舵机测试结果:\r\n"
             "  输入目标位置：角度=%.2f°\r\n"
             "  舵机系统预设位置: 角度=%.2f°, 执行时间=%d毫秒\r\n"
             "  观察得到实际位置: 角度=%.2f°\r\n"
             "  输入-观察的误差: %.2f°\r\n"
             "  预设-观察的误差: %.2f°\r\n"
             "  测试数据: %d,%.2f,%.2f,%.2f\r\n", // ID,目标角度,预设角度,实际角度
             target_angle, 
             preset_angle, preset_time,
             actual_angle,
             input_error,
             preset_error,
             id, target_angle, preset_angle, actual_angle);
    uart_parser_put_string(buf);
}

static void handle_servo_position_delay(int argc, char *argv[])
{
    if (argc < 4) {
        uart_parser_put_string("用法: servo_position_delay <servo_id> <angle> <time_ms>\r\n");
        return;
    }
    ensure_serial_servo_inited();
    uint8_t id = (uint8_t)atoi(argv[1]);
    float angle = (float)atof(argv[2]);
    uint16_t time_ms = (uint16_t)atoi(argv[3]);
    
    if (serialServo.move_servo_with_time_delay(id, angle, time_ms) == Operation_Success) {
        uart_parser_put_string("舵机延时移动指令已设置\r\n");
    } else {
        uart_parser_put_string("设置舵机延时移动失败\r\n");
    }
}

static void handle_servo_get_delay(int argc, char *argv[])
{
    if (argc < 2) {
        uart_parser_put_string("用法: servo_get_delay <servo_id>\r\n");
        return;
    }
    ensure_serial_servo_inited();
    uint8_t id = (uint8_t)atoi(argv[1]);
    float angle = 0;
    uint16_t time_ms = 0;
    char buf[128];
    
    if (serialServo.get_servo_move_with_time_delay(id, angle, time_ms) == Operation_Success) {
        snprintf(buf, sizeof(buf), "Servo %d 延时预设: 角度=%.2f°, 执行时间=%d毫秒\r\n", id, angle, time_ms);
        uart_parser_put_string(buf);
    } else {
        uart_parser_put_string("获取舵机延时预设失败\r\n");
    }
}

static void handle_servo_offset(int argc, char *argv[])
{
    if (argc < 4) {
        uart_parser_put_string("用法: servo_offset <servo_id> <angle> <save>\r\n");
        return;
    }
    ensure_serial_servo_inited();
    uint8_t id = (uint8_t)atoi(argv[1]);
    float angle = (float)atof(argv[2]);
    bool save = (bool)atoi(argv[3]);
    
    if (serialServo.set_servo_angle_offset(id, angle, save) == Operation_Success) {
        char buf[128];
        snprintf(buf, sizeof(buf), "舵机%d角度偏移已设置为%.2f°, %s\r\n", id, angle, save ? "已保存到存储器" : "未保存");
        uart_parser_put_string(buf);
    } else {
        uart_parser_put_string("设置舵机角度偏移失败\r\n");
    }
}

static void handle_servo_get_offset(int argc, char *argv[])
{
    if (argc < 2) {
        uart_parser_put_string("用法: servo_get_offset <servo_id>\r\n");
        return;
    }
    ensure_serial_servo_inited();
    uint8_t id = (uint8_t)atoi(argv[1]);
    float offset = 0;
    char buf[128];
    
    if (serialServo.get_servo_angle_offset(id, offset) == Operation_Success) {
        snprintf(buf, sizeof(buf), "Servo %d 角度偏移: %.2f°\r\n", id, offset);
        uart_parser_put_string(buf);
    } else {
        uart_parser_put_string("获取舵机角度偏移失败\r\n");
    }
}

static void handle_servo_angle_range(int argc, char *argv[])
{
    if (argc < 4) {
        uart_parser_put_string("用法: servo_angle_range <servo_id> <min> <max>\r\n");
        return;
    }
    ensure_serial_servo_inited();
    uint8_t id = (uint8_t)atoi(argv[1]);
    float min_angle = (float)atof(argv[2]);
    float max_angle = (float)atof(argv[3]);
    
    if (serialServo.set_servo_angle_range(id, min_angle, max_angle) == Operation_Success) {
        char buf[128];
        snprintf(buf, sizeof(buf), "舵机%d角度范围已设置为 %.2f° 至 %.2f°\r\n", id, min_angle, max_angle);
        uart_parser_put_string(buf);
    } else {
        uart_parser_put_string("设置舵机角度范围失败\r\n");
    }
}

static void handle_servo_get_angle_range(int argc, char *argv[])
{
    if (argc < 2) {
        uart_parser_put_string("用法: servo_get_range <servo_id>\r\n");
        return;
    }
    ensure_serial_servo_inited();
    uint8_t id = (uint8_t)atoi(argv[1]);
    float min_angle = 0;
    float max_angle = 0;
    char buf[128];
    
    if (serialServo.get_servo_angle_range(id, &min_angle, &max_angle) == Operation_Success) {
        snprintf(buf, sizeof(buf), "Servo %d 角度范围: %.2f° 至 %.2f°\r\n", id, min_angle, max_angle);
        uart_parser_put_string(buf);
    } else {
        uart_parser_put_string("获取舵机角度范围失败\r\n");
    }
}

static void handle_servo_voltage_range(int argc, char *argv[])
{
    if (argc < 4) {
        uart_parser_put_string("用法: servo_voltage_range <servo_id> <min> <max>\r\n");
        return;
    }
    ensure_serial_servo_inited();
    uint8_t id = (uint8_t)atoi(argv[1]);
    float min_vin = (float)atof(argv[2]);
    float max_vin = (float)atof(argv[3]);
    
    if (serialServo.set_servo_vin_range(id, min_vin, max_vin) == Operation_Success) {
        char buf[128];
        snprintf(buf, sizeof(buf), "舵机%d电压范围已设置为 %.2fV 至 %.2fV\r\n", id, min_vin, max_vin);
        uart_parser_put_string(buf);
    } else {
        uart_parser_put_string("设置舵机电压范围失败\r\n");
    }
}


// 计算命令表中的命令总数
static const int num_commands = sizeof(command_table) / sizeof(command_t);


/* -------------------- 3. 核心解析与分派逻辑 -------------------- */

/**
 * @brief 解析并执行命令。
 * @param cmd_string 从队列中接收到的原始命令字符串。
 */
static void process_command(char *cmd_string)
{
    char *argv[UART_PARSER_MAX_ARGS];
    int argc = 0;
    char response_buffer[128]; // 用于格式化输出

    // 使用strtok来分割字符串。注意：strtok会修改原字符串。
    char *token = strtok(cmd_string, " \t\n\r"); // 以空格、制表符、换行符、回车符作为分隔符
    while (token != NULL && argc < UART_PARSER_MAX_ARGS) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\n\r");
    }

    if (argc == 0) {
        return; // 空命令，直接忽略
    }

    // 在命令表中查找匹配的命令
    for (int i = 0; i < num_commands; i++) {
        if (strcmp(argv[0], command_table[i].name) == 0) {
            // 找到命令，调用其处理函数
            command_table[i].handler(argc, argv);
            return;
        }
    }

    // 如果循环结束仍未找到命令
    snprintf(response_buffer, sizeof(response_buffer), "Error: Unknown command '%s'. Type 'help' for a list.\r\n", argv[0]);
    uart_parser_put_string(response_buffer);
}


/* -------------------- 4. FreeRTOS 任务与队列接口 -------------------- */

void uart_parser_task(void *argument)
{
    // 创建消息队列
    // 队列中存储的是指向命令字符串缓冲区的指针
    uart_command_queue = xQueueCreate(UART_PARSER_QUEUE_LENGTH, sizeof(char *));
    
    if (uart_command_queue == NULL) {
        // 队列创建失败，可以在这里处理错误，例如打印日志或进入死循环
        uart_parser_put_string("Fatal Error: Failed to create command queue!\r\n");
        while(1);
    }
    
    char *p_command_buffer;

    uart_parser_put_string("\r\nUART Command Parser Initialized. Type 'help' to start.\r\n> ");

    for (;;) {
        // 永久阻塞等待队列消息
        if (xQueueReceive(uart_command_queue, &p_command_buffer, portMAX_DELAY) == pdPASS) {
            if (p_command_buffer != NULL) {
                // 处理接收到的命令
                process_command(p_command_buffer);
                
                // 提示符
                uart_parser_put_string("> ");

                // 重要：在这里释放用于存储命令字符串的内存。
                // 假设这块内存是在接收端（如UART中断）动态分配的。
                // 如果使用静态缓冲区，则不需要释放。
                // free(p_command_buffer); 
            }
        }
    }
}

int uart_parser_send_command_to_queue(char *cmd_string)
{
    if (uart_command_queue == NULL) {
        return pdFAIL;
    }
    
    // 注意：这里只是将指针发送到队列，而不是整个字符串。
    // 这要求指针指向的内存在被 uart_parser_task 处理之前必须是有效的。
    // 在ISR中调用时，应使用 xQueueSendFromISR。
    // 这里提供一个任务上下文的版本。
    if (xQueueSend(uart_command_queue, &cmd_string, (TickType_t)0) != pdPASS) {
        // 队列已满
        return errQUEUE_FULL;
    }

    return pdPASS;
}

/* -------------------- 5. 命令处理函数的具体实现 -------------------- */

static void handle_help(int argc, char *argv[])
{
    char buffer[128];
    uart_parser_put_string("Available commands:\r\n");
    for (int i = 0; i < num_commands; i++) {
        snprintf(buffer, sizeof(buffer), "  - %s\r\n", command_table[i].help_string);
        uart_parser_put_string(buffer);
    }
}

static void handle_reboot(int argc, char *argv[])
{
    uart_parser_put_string("Rebooting system...\r\n");
    // 延时一小段时间以确保串口消息发送完毕
    vTaskDelay(pdMS_TO_TICKS(100)); 
    // 调用平台相关的重启函数
    esp_restart(); // ESP32重启函数
}

static void handle_get_sys_info(int argc, char *argv[])
{
    char response[128];
    
    // 获取并格式化系统信息
    snprintf(response, sizeof(response),
             "System Info:\r\n"
             "  - IDF Version: %s\r\n"
             "  - CPU Freq: %d MHz\r\n"
             "  - Free Heap: %d bytes\r\n",
             esp_get_idf_version(),
             getCpuFrequencyMhz(),
             esp_get_free_heap_size());
             
    uart_parser_put_string(response);
}

static void handle_get_wifi_status(int argc, char *argv[])
{
    char response[128];

    if (is_wifi_connected()) {
        snprintf(response, sizeof(response), 
                "WiFi Status: Connected\r\n"
                "IP Address: %s\r\n", 
                WiFi.localIP().toString().c_str());
    } else {
        snprintf(response, sizeof(response), "WiFi Status: Disconnected\r\n");
    }
    uart_parser_put_string(response);
}

static void handle_wifi_disconnect(int argc, char *argv[])
{
    if (wifi_disconnect()) {
        uart_parser_put_string("WiFi disconnected successfully.\r\n");
    } else {
        uart_parser_put_string("Failed to disconnect WiFi.\r\n");
    }
}

static void handle_wifi_connect(int argc, char *argv[])
{
    char response[128];
    
    if (argc < 2) {
        uart_parser_put_string("Usage: wifi_connect <ssid> [password]\r\n");
        return;
    }
    
    const char* ssid = argv[1];
    const char* password = (argc >= 3) ? argv[2] : NULL;
    
    snprintf(response, sizeof(response), "Connecting to WiFi: %s...\r\n", ssid);
    uart_parser_put_string(response);
    
    if (wifi_connect_new(ssid, password, 15000)) {
        snprintf(response, sizeof(response), 
                "WiFi connected successfully!\r\n"
                "IP Address: %s\r\n", 
                WiFi.localIP().toString().c_str());
        uart_parser_put_string(response);
    } else {
        uart_parser_put_string("Failed to connect to WiFi.\r\n");
    }
}

static void handle_wifi_config(int argc, char *argv[])
{
    wifi_task_config_t config;
    char response[256];
    
    if (get_current_wifi_config(&config)) {
        snprintf(response, sizeof(response),
                "Current WiFi Configuration:\r\n"
                "  SSID: %s\r\n"
                "  Mode: %s\r\n"
                "  Power Save: %s\r\n"
                "  TX Power: %d\r\n",
                config.ssid,
                (config.wifi_mode == WIFI_STA) ? "Station" : 
                (config.wifi_mode == WIFI_AP) ? "Access Point" : "AP+STA",
                config.power_save ? "Enabled" : "Disabled",
                (int)config.tx_power);
        uart_parser_put_string(response);
    } else {
        uart_parser_put_string("Failed to get WiFi configuration.\r\n");
    }
}

static void handle_network_status(int argc, char *argv[])
{
    char response[128];
    
    if (is_network_connected()) {
        const char* info = get_network_info();
        snprintf(response, sizeof(response), 
                "Network Status: Connected\r\n"
                "Info: %s\r\n", 
                info ? info : "Unknown");
    } else {
        snprintf(response, sizeof(response), "Network Status: Disconnected\r\n");
    }
    uart_parser_put_string(response);
}

static void handle_network_disconnect(int argc, char *argv[])
{
    if (network_disconnect()) {
        uart_parser_put_string("Network disconnected successfully.\r\n");
    } else {
        uart_parser_put_string("Failed to disconnect network.\r\n");
    }
}

static void handle_tcp_connect(int argc, char *argv[])
{
    char response[128];
    
    if (argc != 3) {
        uart_parser_put_string("Usage: tcp_connect <host> <port>\r\n");
        return;
    }
    
    const char* host = argv[1];
    uint16_t port = (uint16_t)atoi(argv[2]);
    
    if (port == 0) {
        uart_parser_put_string("Error: Invalid port number.\r\n");
        return;
    }
    
    snprintf(response, sizeof(response), "Connecting to TCP server %s:%d...\r\n", host, port);
    uart_parser_put_string(response);
    
    if (network_connect_tcp_client(host, port, 10000)) {
        uart_parser_put_string("TCP connection established successfully!\r\n");
    } else {
        uart_parser_put_string("Failed to connect to TCP server.\r\n");
    }
}

static void handle_network_config(int argc, char *argv[])
{
    network_config_t config;
    char response[256];
    
    if (get_current_network_config(&config)) {
        const char* protocol_str;
        switch (config.protocol) {
            case NETWORK_PROTOCOL_TCP_CLIENT:
                protocol_str = "TCP Client";
                break;
            case NETWORK_PROTOCOL_TCP_SERVER:
                protocol_str = "TCP Server";
                break;
            case NETWORK_PROTOCOL_UDP:
                protocol_str = "UDP";
                break;
            default:
                protocol_str = "None";
                break;
        }
        
        snprintf(response, sizeof(response),
                "Current Network Configuration:\r\n"
                "  Protocol: %s\r\n"
                "  Remote Host: %s\r\n"
                "  Remote Port: %d\r\n"
                "  Local Port: %d\r\n"
                "  Auto Connect: %s\r\n",
                protocol_str,
                config.remote_host,
                config.remote_port,
                config.local_port,
                config.auto_connect ? "Enabled" : "Disabled");
        uart_parser_put_string(response);
    } else {
        uart_parser_put_string("Failed to get network configuration.\r\n");
    }
}

static void handle_network_send(int argc, char *argv[])
{
    char response[128];
    
    if (argc < 2) {
        uart_parser_put_string("Usage: network_send <message>\r\n");
        return;
    }
    
    // 重构消息，支持带空格的消息
    char message[256] = {0};
    for (int i = 1; i < argc; i++) {
        if (i > 1) {
            strcat(message, " ");
        }
        strcat(message, argv[i]);
    }
    strcat(message, "\n");
    
    int result = network_send_string(message);
    if (result > 0) {
        snprintf(response, sizeof(response), "Message sent successfully (%d bytes).\r\n", result);
        uart_parser_put_string(response);
    } else {
        uart_parser_put_string("Failed to send message. Check network connection.\r\n");
    }
}

static void handle_wifi_reconnect(int argc, char *argv[])
{
    char response[128];
    wifi_task_config_t config;
    
    // 获取当前配置
    if (!get_current_wifi_config(&config)) {
        uart_parser_put_string("Error: No current WiFi configuration found.\r\n");
        return;
    }
    
    snprintf(response, sizeof(response), "Reconnecting to WiFi: %s...\r\n", config.ssid);
    uart_parser_put_string(response);
    
    // 断开当前连接并重新连接
    wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(1000)); // 等待断开完成
    
    if (wifi_connect_new(config.ssid, config.password, 15000)) {
        snprintf(response, sizeof(response), 
                "WiFi reconnected successfully!\r\n"
                "IP Address: %s\r\n", 
                WiFi.localIP().toString().c_str());
        uart_parser_put_string(response);
    } else {
        uart_parser_put_string("Failed to reconnect to WiFi.\r\n");
    }
}

static void handle_network_reconnect(int argc, char *argv[])
{
    char response[128];
    network_config_t config;
    
    // 获取当前网络配置
    if (!get_current_network_config(&config)) {
        uart_parser_put_string("Error: No current network configuration found.\r\n");
        return;
    }
    
    // 检查是否有有效的网络配置
    if (config.protocol == NETWORK_PROTOCOL_NONE) {
        uart_parser_put_string("Error: No network protocol configured.\r\n");
        return;
    }
    
    uart_parser_put_string("Reconnecting to network...\r\n");
    
    // 断开当前网络连接
    network_disconnect();
    vTaskDelay(pdMS_TO_TICKS(500)); // 等待断开完成
    
    // 根据协议类型重新连接
    switch (config.protocol) {
        case NETWORK_PROTOCOL_TCP_CLIENT:
            if (network_connect_tcp_client(config.remote_host, config.remote_port, 10000)) {
                uart_parser_put_string("Network reconnected successfully!\r\n");
            } else {
                uart_parser_put_string("Failed to reconnect to network.\r\n");
            }
            break;
            
        case NETWORK_PROTOCOL_TCP_SERVER:
        case NETWORK_PROTOCOL_UDP:
            uart_parser_put_string("Note: Server/UDP modes don't require active reconnection.\r\n");
            // 对于服务器模式和UDP，可以考虑重新初始化
            break;
            
        default:
            uart_parser_put_string("Error: Unsupported protocol for reconnection.\r\n");
            break;
    }
}

/* -------------------- 6. 平台相关的硬件接口 (需要用户实现) -------------------- */

/**
 * @brief 这是一个需要您在项目中具体实现的函数。
 * 它的作用是通过UART发送一个字符串。
 */
__attribute__((weak)) void uart_parser_put_string(const char *str)
{
    // 示例实现 (基于STM32 HAL库):
    // extern UART_HandleTypeDef huart1;
    // HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);

    // 如果没有实现，这个弱定义函数将什么也不做，避免链接错误。
    (void)str;
}
