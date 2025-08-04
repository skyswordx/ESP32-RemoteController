#ifndef UART_PARSER_H
#define UART_PARSER_H
/**
 * @file uart_parser.h
 * @brief UART Parser Header File
 * @author circlemoon
 * @date 2025-08-03
 */

#include <stdint.h>

/* * 为了兼容C++工程，使用 extern "C" 宏。
 * 在C++编译器下，它会告诉编译器以C语言的方式来链接这些函数。
 * 在C编译器下，这个宏会被忽略。
 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 定义命令处理函数的函数指针类型。
 * @param argc 参数个数 (包括命令本身)。
 * @param argv 参数数组，argv[0]是命令本身，后续是参数。
 */
typedef void (*command_handler_t)(int argc, char *argv[]);

/**
 * @brief 定义命令表的条目结构体。
 */
typedef struct {
    const char *name;           /**< 命令的名称 (用户输入的字符串) */
    command_handler_t handler;  /**< 指向该命令处理函数的指针 */
    const char *help_string;    /**< 命令的帮助信息，用于 'help' 命令 */
} command_t;


/**
 * @brief FreeRTOS 任务函数，用于处理UART命令解析。
 *
 * @note  这个任务会持续等待从串口接收到的命令，然后解析并分派给对应的处理函数。
 * 它依赖于一个消息队列来从UART中断服务程序(ISR)接收命令字符串。
 * @param argument 传递给任务的参数 (未使用)。
 */
void uart_parser_task(void *argument);


/**
 * @brief 将从UART接收到的完整命令字符串发送到解析任务。
 *
 * @note  这个函数应该在UART接收中断中被调用，当检测到一个完整的命令
 * (例如，以换行符 '\n' 结尾)时。
 * 为了线程安全，尤其是在ISR中使用时，它应该通过线程安全的方式
 * (如FreeRTOS的消息队列)将数据发送给 uart_parser_task。
 *
 * @param cmd_string 指向包含完整命令的字符串缓冲区的指针。
 * @return int 如果成功发送到队列，返回 pdPASS，否则返回 errQUEUE_FULL。
 */
int uart_parser_send_command_to_queue(char *cmd_string);


/**
 * @brief 平台相关的UART输出函数 (需要用户实现)。
 *
 * @note  这个函数用于将解析器的响应或日志信息通过UART发送出去。
 * 这是为了将核心解析逻辑与底层硬件驱动解耦。
 * 您需要在您的项目中实现这个函数的具体内容。
 *
 * @param str 要发送的字符串。
 */
void uart_parser_put_string(const char *str);


#ifdef __cplusplus
}
#endif

#endif /* UART_PARSER_H */
