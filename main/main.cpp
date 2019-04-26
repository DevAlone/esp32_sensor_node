#include "globals.h"
#include "process_sensors.h"
#include "sensor_node.h"

#include "mdf_common.h"
#include "mwifi.h"

// log tag
static const char* TAG = "esp32_sensor_node";

extern "C" {
void app_main(void);
}

#if CONFIG_USE_MESH == true
// #define MEMORY_DEBUG

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

        static bool rootWriteWorkerStarted = false;
        if (!rootWriteWorkerStarted) {
            xTaskCreate(
                rootWriteWorker,
                "rootWriteWorker",
                4 * 1024,
                nullptr,
                CONFIG_MDF_TASK_DEFAULT_PRIOTY,
                nullptr);
            rootWriteWorkerStarted = true;
        }
        break;
    }

    default:
        break;
    }

    return MDF_OK;
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
        meshNodeWriteWorker,
        "nodeWriteWorker",
        4 * 1024,
        nullptr,
        CONFIG_MDF_TASK_DEFAULT_PRIOTY,
        nullptr);

    xTaskCreate(
        meshNodeWriteWorker,
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
#else
static EventGroupHandle_t wifi_event_group;
const static int CONNECTED_BIT = BIT0;

static esp_err_t wifi_event_handler(void*, system_event_t* event)
{
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
        isWifiInitialized = true;
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);

        static bool rootWriteWorkerStarted = false;
        if (!rootWriteWorkerStarted) {
            xTaskCreate(
                rootWriteWorker,
                "rootWriteWorker",
                4 * 1024,
                nullptr,
                CONFIG_MDF_TASK_DEFAULT_PRIOTY,
                nullptr);
            rootWriteWorkerStarted = true;
        }

        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
        isWifiInitialized = false;
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void wifi_init(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {};
    wifi_config.sta = {};
    strcpy((char*)wifi_config.sta.ssid, CONFIG_ROUTER_SSID);
    strcpy((char*)wifi_config.sta.password, CONFIG_ROUTER_PASSWORD);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_LOGI(TAG, "start the WIFI SSID:[%s] password:[%s]", CONFIG_ROUTER_SSID, "******");
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Waiting for wifi");
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
}

void app_main()
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    nvs_flash_init();
    wifi_init();

    /*
    TimerHandle_t timer = xTimerCreate(
        "printSystemInfo",
        10000 / portTICK_RATE_MS,
        true,
        nullptr,
        printSystemInfo);
    xTimerStart(timer, 0);
    */
}
#endif
