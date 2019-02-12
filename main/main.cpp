#include "mqtt.h"
#include "process_sensors.h"
#include "sensor_node.h"

#include "mdf_common.h"
#include "mwifi.h"

// #define MEMORY_DEBUG

// log tag
static const char* TAG = "esp32_sensor_node";

static mdf_err_t wifi_init()
{
    mdf_err_t ret = nvs_flash_init();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        MDF_ERROR_ASSERT(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    MDF_ERROR_ASSERT(ret);

    tcpip_adapter_init();
    MDF_ERROR_ASSERT(esp_event_loop_init(NULL, NULL));
    MDF_ERROR_ASSERT(esp_wifi_init(&cfg));
    MDF_ERROR_ASSERT(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    MDF_ERROR_ASSERT(esp_wifi_set_mode(WIFI_MODE_STA));
    MDF_ERROR_ASSERT(esp_wifi_set_ps(WIFI_PS_NONE));
    MDF_ERROR_ASSERT(esp_mesh_set_6m_rate(false));
    MDF_ERROR_ASSERT(esp_wifi_start());

    return MDF_OK;
}

/**
 * @brief All module events will be sent to this task in esp-mdf
 *
 * @Note:
 *     1. Do not block or lengthy operations in the callback function.
 *     2. Do not consume a lot of memory in the callback function.
 *        The task memory of the callback function is only 4KB.
 */
static mdf_err_t event_loop_cb(mdf_event_loop_t event, void*)
{
    MDF_LOGI("event_loop_cb, event: %d", event);

    switch (event) {
    case MDF_EVENT_MWIFI_STARTED:
        MDF_LOGI("MESH is started");
        break;

    case MDF_EVENT_MWIFI_PARENT_CONNECTED:
        MDF_LOGI("Parent is connected on station interface");
        break;

    case MDF_EVENT_MWIFI_PARENT_DISCONNECTED:
        MDF_LOGI("Parent is disconnected on station interface");
        break;

    case MDF_EVENT_MWIFI_ROUTING_TABLE_ADD:
    case MDF_EVENT_MWIFI_ROUTING_TABLE_REMOVE:
        MDF_LOGI("routing table has been changed, total_num: %d", esp_mesh_get_total_node_num());
        break;

    case MDF_EVENT_MWIFI_ROOT_GOT_IP: {
        MDF_LOGI("Root obtains the IP address. It is posted by LwIP stack automatically");
        xTaskCreate(
            rootWriteWorker,
            "rootWriteWorker",
            4 * 1024,
            nullptr,
            CONFIG_MDF_TASK_DEFAULT_PRIOTY,
            nullptr);
        /*
        xTaskCreate(tcp_client_read_task, "tcp_server_read", 4 * 1024,
            NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);
        */
        mqttInit(); // TODO: remove
        break;
    }

    default:
        break;
    }

    return MDF_OK;
}

extern "C" {
void app_main(void);
}

void app_main()
{
    mwifi_init_config_t cfg = MWIFI_INIT_CONFIG_DEFAULT();
    mwifi_config_t config = {};
    strcpy(config.router_ssid, CONFIG_ROUTER_SSID);
    strcpy(config.router_password, CONFIG_ROUTER_PASSWORD);
    strcpy((char*)config.mesh_id, CONFIG_MESH_ID);
    strcpy(config.mesh_password, CONFIG_MESH_PASSWORD);

    // Set the log level for serial port printing.
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);

    gpio_pad_select_gpio(CONFIG_LED_GPIO_NUM);
    gpio_set_direction(gpio_num_t(CONFIG_LED_GPIO_NUM), GPIO_MODE_INPUT_OUTPUT);

    // Initialize wifi mesh.
    MDF_ERROR_ASSERT(mdf_event_loop_init(event_loop_cb));
    MDF_ERROR_ASSERT(wifi_init());
    MDF_ERROR_ASSERT(mwifi_init(&cfg));
    MDF_ERROR_ASSERT(mwifi_set_config(&config));
    MDF_ERROR_ASSERT(mwifi_start());

    // Create tasks
    xTaskCreate(
        nodeWriteWorker,
        "nodeWriteWorker",
        4 * 1024,
        nullptr,
        CONFIG_MDF_TASK_DEFAULT_PRIOTY,
        nullptr);

    xTaskCreate(
        nodeReadWorker,
        "nodeReadWorker",
        4 * 1024,
        nullptr,
        CONFIG_MDF_TASK_DEFAULT_PRIOTY,
        nullptr);

    TimerHandle_t timer = xTimerCreate(
        "printSystemInfo",
        10000 / portTICK_RATE_MS,
        true,
        nullptr,
        printSystemInfo);
    xTimerStart(timer, 0);
}
