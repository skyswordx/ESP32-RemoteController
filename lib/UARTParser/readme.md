# 命令分派表框架集成与使用指南

本文档将指导您如何将 uart_parser 模块（包含 uart_parser.c 和 uart_parser.h）集成到您现有的嵌入式项目中，并利用它来构建一个灵活、可扩展的串口调试系统。

## 步骤 0: 文件准备与项目配置
在开始之前，请确保您已经将上一节中提供的 uart_parser.c 和 uart_parser.h 文件添加到了您的IDE工程中。

- **添加源文件**: 将 `uart_parser.c` 添加到您项目的源文件列表（Source/src）中，以便它能被编译器找到并编译。
- **包含头文件**: 在需要调用此模块功能的文件中（例如，`main.c` 或您专门用于处理串口中断的文件），包含头文件 `#include "uart_parser.h"`。
- **确认依赖**: 确保您的项目已经正确配置了 FreeRTOS 和 STM32 HAL 库。

## 步骤 1: 实现硬件接口层 (关键解耦步骤)
为了让 `uart_parser` 模块保持平台无关性，我们需要实现它与具体硬件交互的“桥梁”。
**（统一下面以 STM32 HAL API 为例，实际您需要灵活调整）**

### 1.1 实现串口发送函数 `uart_parser_put_string`
这个函数负责将解析器的输出（如响应信息、错误提示）通过物理串口发送出去。

1.  打开 `uart_parser.c` 文件。
2.  找到文件末尾的 `uart_parser_put_string` 函数。它被定义为弱函数 `__attribute__((weak))`，允许我们在项目其他地方提供一个强定义来覆盖它。
3.  在您的某个源文件中 (例如 `main.c`)，添加以下强定义实现（统一下面以 STM32 HAL API 为例，实际您需要灵活调整）：

```c
#include "uart_parser.h"
#include <string.h>

// 假设您使用 huart1 进行调试输出
extern UART_HandleTypeDef huart1; 

void uart_parser_put_string(const char *str)
{
    // 直接调用您熟悉的STM32 HAL库函数来发送字符串
    HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}
```

### 1.2 实现串口接收与命令入队
这是将用户输入送入解析任务的关键。我们需要修改您的UART中断回调函数，将接收到的完整命令发送到 `uart_parser` 的消息队列中。

在您处理UART中断的文件中 (例如 `my_uart_task.c` 或 `stm32f4xx_it.c`)，进行如下修改：

```c
#include "uart_parser.h"

// 定义一个静态缓冲区和计数器用于接收数据
#define RX_BUFFER_SIZE 64 // 与 uart_parser 中的最大命令长度保持一致或更大
static char rx_buffer[RX_BUFFER_SIZE];
static uint8_t rx_counter = 0;

// 修改您的 HAL_UART_RxCpltCallback 函数
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    // 假设您的调试串口是 huart1
    if (huart->Instance == USART1) {
        // 检查接收到的字符
        uint8_t received_char = huart->Instance->DR; // 直接从数据寄存器读取

        // 判断是否是命令结束符 (回车或换行)
        if (received_char == '\r' || received_char == '\n') {
            if (rx_counter > 0) {
                // 命令接收完毕
                rx_buffer[rx_counter] = '\0'; // 添加字符串结束符

                // 将接收到的命令字符串发送到解析任务的队列
                // 注意：这里我们直接传递了静态缓冲区的地址。
                // 这是一个简化处理，在更复杂的系统中可能需要动态分配或使用更复杂的缓冲区管理。
                uart_parser_send_command_to_queue(rx_buffer);

                // 重置计数器，准备接收下一条命令
                rx_counter = 0;
            }
            // 可以在这里回显换行符，提供更好的终端体验
            // HAL_UART_Transmit(huart, (uint8_t*)"\r\n", 2, 100);

        } else if (received_char == '\b' || received_char == 127) { // 处理退格键
            if (rx_counter > 0) {
                rx_counter--;
                // 在终端上回显退格、空格、退格，以实现删除效果
                // HAL_UART_Transmit(huart, (uint8_t*)"\b \b", 3, 100);
            }
        } else {
            // 将字符存入缓冲区
            if (rx_counter < RX_BUFFER_SIZE - 1) {
                rx_buffer[rx_counter++] = received_char;
                // 可以在这里回显接收到的字符
                // HAL_UART_Transmit(huart, &received_char, 1, 100);
            }
        }
        
        // 再次使能串口接收中断。在新的HAL库中，这个可能不再需要手动调用，
        // 但为了兼容性，保留此逻辑。请根据您的HAL库版本确认。
        // HAL_UART_Receive_IT(huart, &aRxBuffer, 1); // aRxBuffer 此时不再直接使用
    }
}
```

> **重要**: 您需要确保在初始化时，已经启动了UART的接收中断。
> ```c
> // HAL_UART_Receive_IT(&huart1, &some_dummy_byte, 1);
> ```
> 在上面的回调函数中，我们不再依赖 `aRxBuffer`，而是直接从DR寄存器读取，这样可以避免一些HAL库版本中的中断处理问题。

**注意**: 上述代码直接使用了静态缓冲区`rx_buffer`。这意味着在`uart_parser_task`处理完命令之前，中断不能覆盖这个缓冲区。由于队列和任务切换速度很快，对于手动输入的调试命令，这通常是安全的。

## 步骤 2: 创建并启动解析器任务
在您的 `main.c` 文件中，找到FreeRTOS任务创建的部分，添加 `uart_parser_task` 的创建代码。

在 `main()` 函数中，初始化完硬件和外设之后，创建任务:
```c
int main(void)
{
    // ... HAL_Init(), SystemClock_Config(), MX_GPIO_Init(), etc. ...

    // 创建 uart_parser 任务
    xTaskCreate(
        uart_parser_task,           // 任务函数
        "UART_Parser_Task",         // 任务名
        256,                        // 任务栈大小 (word)
        NULL,                       // 传递给任务的参数
        tskIDLE_PRIORITY + 2,       // 任务优先级
        NULL                        // 任务句柄
    );

    // ... 创建您的其他任务 ...

    // 启动调度器
    vTaskStartScheduler();

    while (1)
    {
    }
}
```

## 步骤 3: 添加您的第一个自定义命令
现在，我们将您旧代码中的一个功能——设置DDS频率，迁移到新的框架下。我们将创建一个新命令 `set_dds_freq <frequency>`。

### 3.1 编写命令处理函数
打开 `uart_parser.c` 文件。在文件顶部的“命令处理函数的实现”区域，添加一个新的静态函数：

```c
// 包含您需要用到的头文件
#include "AD9954.h" // 假设这是您控制DDS的函数所在头文件

static void handle_set_dds_freq(int argc, char *argv[])
{
    // 1. 校验参数个数是否正确
    if (argc != 2) {
        uart_parser_put_string("Usage: set_dds_freq <frequency_in_hz>\r\n");
        return;
    }

    // 2. 将字符串参数转换为数字
    // strtoul 是一个比 atoi 更健壮的选择，可以处理更大的无符号数
    uint32_t freq_hz = (uint32_t)strtoul(argv[1], NULL, 10);

    // 3. 调用您底层的硬件驱动函数
    AD9954_Set_Fre(freq_hz); // 调用您原有的函数

    // 4. (可选) 返回一个确认信息
    char response[64];
    snprintf(response, sizeof(response), "OK. DDS frequency set to %lu Hz.\r\n", freq_hz);
    uart_parser_put_string(response);
}
```

### 3.2 注册新命令
仍然在 `uart_parser.c` 文件中，找到 `command_table[]` 数组。在数组中添加一行来注册我们刚刚创建的函数：

```c
static const command_t command_table[] = {
    /* 命令名       处理函数指针      帮助信息字符串 */
    {"help",      handle_help,     "help: 显示所有可用命令。"},
    {"reboot",    handle_reboot,   "reboot: 重启设备。"},
    {"set_led",   handle_set_led,  "set_led <id> <on|off>: 设置LED状态。"},
    {"get_temp",  handle_get_temp, "get_temp <id>: 读取温度传感器。"},
    /* --- 在此行下方添加您的新命令 --- */
    {"set_dds_freq", handle_set_dds_freq, "set_dds_freq <freq_hz>: 设置DDS输出频率。"},
};
```

## 步骤 4: 编译、烧录与测试
1.  重新编译您的整个工程并烧录到目标板。
2.  打开一个串口终端工具（如PuTTY, Tera Term, 或VSCode的Serial Monitor）。
3.  配置好正确的COM口和波特率，连接到您的设备。
4.  按下设备的复位按钮，您应该会看到欢迎信息：
    ```
    UART Command Parser Initialized. Type 'help' to start.
    > 
    ```
5.  开始测试：
    -   输入 `help` 并按回车，您应该能看到包括 `set_dds_freq` 在内的所有命令列表。
    -   输入 `set_dds_freq 100000` 并按回车，您应该会看到确认信息 `OK. DDS frequency set to 100000 Hz.`，并且您的DDS硬件也应该已经更新了频率。
    -   尝试输入一个错误的命令，如 `hello`，系统会提示未知命令。
    -   尝试一个参数错误的命令，如 `set_dds_freq`，系统会提示正确的用法。

至此，您已成功将命令分派表框架集成到您的项目中。现在，您可以按照步骤3的方法，轻松地将您所有的调试功能逐一迁移到这个新的、更加清晰和易于维护的框架下。