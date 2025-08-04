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

/* 宏定义 */
#define UART_PARSER_QUEUE_LENGTH    8      // 命令队列深度
#define UART_PARSER_MAX_CMD_LEN     64     // 支持的最大命令长度
#define UART_PARSER_MAX_ARGS        8      // 支持的最大参数数量

/* FreeRTOS 相关的句柄 */
static QueueHandle_t uart_command_queue = NULL; // 用于接收命令字符串指针的消息队列

/* -------------------- 1. 命令处理函数的实现 -------------------- */
// 在这里添加您的命令处理函数。

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


/* -------------------- 2. 命令分派表 -------------------- */
// 在这里将您的命令和处理函数关联起来。

static const command_t command_table[] = {
    /* 命令名           处理函数指针              帮助信息字符串 */
    {"help",            handle_help,            "help: 显示所有可用命令。"},
    {"reboot",          handle_reboot,          "reboot: 重启设备。"},
    {"get_sys_info",    handle_get_sys_info,    "get_sys_info: 获取系统信息。"},
    {"get_wifi_status", handle_get_wifi_status, "get_wifi_status: 获取WiFi连接状态。"},
    /* --- 您可以在此行下方添加您的新命令 --- */
    
};

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
    char response[64];

    if (is_wifi_connected()) {
        snprintf(response, sizeof(response), "WiFi Status: Connected\r\n");
    } else {
        snprintf(response, sizeof(response), "WiFi Status: Disconnected\r\n");
    }
    uart_parser_put_string(response);
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
