# 混合式分层架构集成指南

本文档将指导您如何将 `data_service` 模块集成到您的 FreeRTOS 项目中，并围绕它构建一个健壮、可扩展的嵌入式应用程序。

## Prompts

我有一个基于数据分拣器+事件组响应的传感器处理架构，我需要你根据这个架构集成到我目前的系统中，并修改掉其中我系统目前没有的传感器，我系统中目前有一个编码器encoder，需要你基于我的encoder驱动(ESP32Encoder这个库)进行数据周期性读取和发送，读取当前ESP32Encoder的值，并以json形式通过wifi tcp发送出去

## 准备工作：添加架构文件

在开始之前，请确保您已经拥有以下两个文件：
- `data_service.h`
- `data_service.c`

### 操作步骤

1. 将 `data_service.h` 放入您项目的核心头文件目录（例如 `Core/Inc`、`include` 等）
2. 将 `data_service.c` 放入您项目的核心源文件目录（例如 `Core/Src`、`source` 等）
3. 将这两个文件添加到您的 IDE 工程或 Makefile 的编译列表中，确保它们能被正常编译和链接

## 第1步：在系统启动时初始化数据服务

`data_service` 模块是整个架构的核心，必须在任何其他任务使用它之前完成初始化。最佳的初始化位置是在 `main()` 函数中，创建任何应用任务之前。

### 操作步骤

1. 在您的 `main.c` 文件中包含头文件 `#include "data_service.h"`
2. 找到创建任务的代码段，在它们之前调用 `data_service_init()`

### 示例代码

```c
/* main.c */
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

// 包含我们的核心服务头文件
#include "data_service.h" 
// 包含您其他模块的头文件
#include "uart_parser.h" 

// 假设您在这里定义了任务函数原型
void temperature_sensor_task(void *argument);
void communication_task(void *argument);

int main(void)
{
    // ... 系统时钟、HAL库等初始化 ...

    // --- 关键步骤 ---
    // 初始化数据服务层
    if (data_service_init() != pdPASS) {
        // 严重错误：核心服务初始化失败
        // 可以在这里处理错误，例如点亮一个错误LED并进入死循环
        Error_Handler();
    }

    // --- 创建您的应用任务 ---
    // 1. 创建传感器任务 (数据生产者)
    xTaskCreate(temperature_sensor_task, "TempTask", 256, NULL, tskIDLE_PRIORITY + 1, NULL);

    // 2. 创建应用任务 (数据消费者)
    xTaskCreate(communication_task, "CommTask", 512, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(uart_parser_task, "UartParserTask", 512, NULL, tskIDLE_PRIORITY + 2, NULL);

    // 启动FreeRTOS调度器
    vTaskStartScheduler();

    // 程序不应该执行到这里
    while (1)
    {
    }
}
```

## 第2步：创建传感器任务（数据生产者）

传感器任务的职责非常单一：从硬件读取数据，然后调用 `data_service` 的更新接口。

### 操作步骤

创建一个或多个传感器任务。以下是一个温湿度传感器任务的范例。

### 示例代码

```c
/* temperature_sensor_task.c */
#include "FreeRTOS.h"
#include "task.h"
#include "data_service.h"

// --- 移植点 ---
// 您需要实现这个底层硬件读取函数
// 它可能在 bsp_dht11.c 或类似文件中
extern bool bsp_read_dht11(float *temp, float *humid); 

void temperature_sensor_task(void *argument)
{
    float temperature;
    float humidity;

    // 使用vTaskDelay来实现周期性执行
    const TickType_t xFrequency = pdMS_TO_TICKS(2000); // 每2秒读取一次
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) 
    {
        // 1. 精确周期延时
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // 2. 调用底层驱动读取硬件数据
        if (bsp_read_dht11(&temperature, &humidity)) {
            // 3. 读取成功，将数据提交给数据服务层
            data_service_update_temp_humid(temperature, humidity);
        } else {
            // 处理读取失败的情况，例如可以更新一个错误状态
        }
    }
}
```

## 第3步：创建应用任务（数据消费者）

应用任务从 `data_service` 获取数据并执行具体业务逻辑。

### 范例A：周期性发送任务（您的核心需求）

这个任务定时从数据服务层拉取最新的系统状态快照，并将其发送出去。

```c
/* communication_task.c */
#include "FreeRTOS.h"
#include "task.h"
#include "data_service.h"
#include <stdio.h>

// --- 移植点 ---
// 您需要实现这个底层发送函数
extern void bsp_send_data_packet(const uint8_t *data, uint16_t len);

// 数据打包函数 (示例)
uint16_t pack_system_state(const system_state_t *state, uint8_t *buffer) {
    // 在这里实现您的通信协议，将 state 结构体打包到 buffer 中
    // 为了演示，我们使用 snprintf 将其格式化为字符串
    return snprintf((char*)buffer, 256, 
        "Temp:%.1f, Humid:%.1f, Accel:(%.2f,%.2f,%.2f)\r\n",
        state->temperature, state->humidity,
        state->imu_data.accel_x, state->imu_data.accel_y, state->imu_data.accel_z
    );
}

void communication_task(void *argument)
{
    system_state_t current_state; // 用于存储系统状态的本地副本
    uint8_t tx_buffer[256];       // 发送缓冲区
    uint16_t packet_len;

    const TickType_t xFrequency = pdMS_TO_TICKS(100); // 固定每100ms发送一次
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;)
    {
        // 1. 精确周期延时
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // 2. 从数据服务层获取最新的系统状态快照
        data_service_get_system_state(&current_state);

        // 3. 将数据打包成协议格式
        packet_len = pack_system_state(&current_state, tx_buffer);

        // 4. 调用底层接口发送数据
        if (packet_len > 0) {
            bsp_send_data_packet(tx_buffer, packet_len);
        }
    }
}
```

### 范例B：事件驱动任务（例如高温报警）

这个任务平时处于休眠状态，只有在温度数据被更新的那一刻才会被唤醒执行。

```c
/* alarm_task.c */
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#include "data_service.h"

// --- 移植点 ---
extern void bsp_activate_buzzer(bool is_on); // 硬件相关的蜂鸣器驱动

#define HIGH_TEMP_THRESHOLD 50.0f

void alarm_task(void *argument)
{
    system_state_t current_state;
    EventGroupHandle_t system_events = data_service_get_event_group_handle();

    for (;;)
    {
        // 1. 等待"温度已更新"事件，永久阻塞，不消耗CPU
        xEventGroupWaitBits(
            system_events,                // 事件组句柄
            BIT_EVENT_TEMP_HUMID_UPDATED, // 等待的事件位
            pdTRUE,                       // 退出时清除该事件位
            pdFALSE,                      // 不用等待所有位 (OR逻辑)
            portMAX_DELAY                 // 永久等待
        );

        // --- 当代码执行到这里，说明温度刚刚被更新 ---
        
        // 2. 获取最新数据
        data_service_get_system_state(&current_state);

        // 3. 执行业务逻辑
        if (current_state.temperature > HIGH_TEMP_THRESHOLD) {
            bsp_activate_buzzer(true);
        } else {
            bsp_activate_buzzer(false);
        }
    }
}
```

## 第4步：与您现有的 uart_parser 模块集成

让您的命令行工具能够查询系统状态，是展示架构威力的一个绝佳方式。

### 操作步骤

在您的 `uart_parser.c` 中添加一个新命令 `get_status`。

### 修改示例

```c
/* uart_parser.c */

// ... 包含其他头文件 ...
#include "data_service.h" // 1. 包含头文件

// ... 其他 handle_... 函数 ...

// 2. 添加新命令的处理函数原型
static void handle_get_status(int argc, char *argv[]);

// 3. 在命令表中注册新命令
static const command_t command_table[] = {
    {"help",      handle_help,     "help: Show all available commands."},
    {"reboot",    handle_reboot,   "reboot: Reboot the device."},
    // ... 其他命令 ...
    {"get_data_status", handle_get_status, "get_data_status: Print the current data system status."}, // 新增命令
};

// ... 其他代码 ...

// 4. 实现新命令的处理函数
static void handle_get_data_status(int argc, char *argv[])
{
    system_state_t current_state;
    char buffer[256];

    // 从数据服务层获取最新状态
    data_service_get_system_state(&current_state);

    // 格式化输出
    snprintf(buffer, sizeof(buffer),
        "--- System Status ---\r\n"
        "Temperature: %.2f C\r\n"
        "Humidity:    %.2f %%\r\n"
        "IMU Accel X: %.3f\r\n"
        "IMU Accel Y: %.3f\r\n"
        "IMU Accel Z: %.3f\r\n"
        "GPS Latitude:  %.6f\r\n"
        "GPS Longitude: %.6f\r\n"
        "-----------------------\r\n",
        current_state.temperature,
        current_state.humidity,
        current_state.imu_data.accel_x,
        current_state.imu_data.accel_y,
        current_state.imu_data.accel_z,
        current_state.gps_data.latitude,
        current_state.gps_data.longitude
    );

    // 使用您模块自己的输出函数
    uart_parser_put_string(buffer);
}
```

现在，当您通过串口发送 `get_data_status` 命令时，系统就会打印出所有传感器的最新数据！

## 第5步：如何扩展——添加一个新传感器

这正是此架构的魅力所在。假设您要添加一个气压传感器 (BMP280)：

### 扩展步骤

1. **修改 `data_service.h`：**
   - 在 `system_state_t` 结构体中添加 `float pressure;`
   - 添加一个新的事件位 `#define BIT_EVENT_PRESSURE_UPDATED (1 << 4)`
   - 声明一个新函数 `void data_service_update_pressure(float pressure);`

2. **修改 `data_service.c`：**
   - 实现 `data_service_update_pressure()` 函数，逻辑和更新温度的函数一样

3. **创建新任务：**
   - 创建一个 `pressure_sensor_task`，让它读取 BMP280 的数据，并调用 `data_service_update_pressure()`

### 完成！

您的 `communication_task` 和 `get_status` 命令会自动地开始处理和显示新的气压数据（您只需在打包和打印函数中加上新字段即可），而无需修改它们的核心逻辑。

---

至此，您已成功将一个高度模块化、可扩展的架构集成到了您的项目中。