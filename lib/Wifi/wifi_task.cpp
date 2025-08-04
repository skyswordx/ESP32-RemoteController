#include "wifi_task.h"
#include "WiFi.h"

#define WIFI_TASK_TAG "WIFI_TASK"

static wifi_task_config_t* s_wifi_config = NULL;
static bool s_is_connected = false;

static void wifi_task_handler(void *pvParameters)
{
    s_wifi_config = (wifi_task_config_t*)pvParameters;

    // A small delay to help prevent brownout if power supply is marginal
    vTaskDelay(pdMS_TO_TICKS(200));

    ESP_LOGI(WIFI_TASK_TAG, "Starting WiFi Task (Arduino)...");

    // Set WiFi Mode
    WiFi.mode(s_wifi_config->wifi_mode);
    ESP_LOGI(WIFI_TASK_TAG, "WiFi mode set to: %d", s_wifi_config->wifi_mode);

    // Set power save mode
    // Note: In Arduino, this is typically managed with WiFi.setSleep(true/false)
    // We will map the boolean flag to this.
    WiFi.setSleep(s_wifi_config->power_save);
    ESP_LOGI(WIFI_TASK_TAG, "Power save mode: %s", s_wifi_config->power_save ? "ON" : "OFF");

    // Set transmit power
    WiFi.setTxPower(s_wifi_config->tx_power);
    ESP_LOGI(WIFI_TASK_TAG, "TX Power set to: %d", (int)s_wifi_config->tx_power);


    if (s_wifi_config->wifi_mode == WIFI_STA || s_wifi_config->wifi_mode == WIFI_AP_STA) {
        ESP_LOGI(WIFI_TASK_TAG, "Connecting to STA: %s", s_wifi_config->ssid);
        WiFi.begin(s_wifi_config->ssid, s_wifi_config->password);

        uint32_t start_time = millis();
        while (WiFi.status() != WL_CONNECTED) {
            vTaskDelay(pdMS_TO_TICKS(500));
            ESP_LOGI(WIFI_TASK_TAG, ".");
            if (millis() - start_time > s_wifi_config->sta_connect_timeout_ms) {
                ESP_LOGE(WIFI_TASK_TAG, "Connection Timeout!");
                break;
            }
        }
    }

    if (s_wifi_config->wifi_mode == WIFI_AP || s_wifi_config->wifi_mode == WIFI_AP_STA) {
        ESP_LOGI(WIFI_TASK_TAG, "Starting AP: %s", s_wifi_config->ap_ssid);
        WiFi.softAP(s_wifi_config->ap_ssid, s_wifi_config->ap_password);
        IPAddress myIP = WiFi.softAPIP();
        ESP_LOGI(WIFI_TASK_TAG, "AP IP address: %s", myIP.toString().c_str());
    }

    if (WiFi.status() == WL_CONNECTED) {
        s_is_connected = true;
        ESP_LOGI(WIFI_TASK_TAG, "WiFi Connected. IP Address: %s", WiFi.localIP().toString().c_str());
    } else {
        s_is_connected = false;
        ESP_LOGW(WIFI_TASK_TAG, "WiFi connection failed or not in STA mode.");
    }

    // The task can now be deleted as WiFi connection is managed by the system
    vTaskDelete(NULL);
}

BaseType_t wifi_task_start(wifi_task_config_t *config)
{
    if (config == NULL) {
        return pdFAIL;
    }

    // Copy config to a static variable to be safely accessed by the task
    static wifi_task_config_t task_config;
    memcpy(&task_config, config, sizeof(wifi_task_config_t));

    return xTaskCreate(
        wifi_task_handler,
        "wifi_task_arduino",
        4096, // Stack size
        (void*)&task_config,
        5,    // Priority
        NULL
    );
}

bool is_wifi_connected(void)
{
    // For STA mode, we can rely on WiFi.isConnected()
    s_is_connected = WiFi.isConnected();
    return s_is_connected;
}
