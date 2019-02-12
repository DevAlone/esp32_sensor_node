#include "mqtt.h"

#include "Defer.h"
#include "helpers.h"

#include "esp_log.h"
// #include "lwip/dns.h"
//#include "lwip/netdb.h"
// #include "lwip/sockets.h"
#include "mdf_common.h"
#include "mwifi.h"

#include <queue>
#include <string>

static const char* LOG_TAG = "MQTT_LOG";

static esp_mqtt_client_handle_t client = nullptr;
static bool isClientReady = false;
static std::queue<std::pair<std::string, std::string>> messagesQueue;

void mqttInit()
{
    esp_mqtt_client_config_t mqttConfig = {};
    mqttConfig.uri = "mqtt://" CONFIG_MQTT_USERNAME ":" CONFIG_MQTT_PASSWORD "@" CONFIG_MQTT_IP ":" CONFIG_MQTT_PORT;
    // mqttConfig.uri = "mqtts://api.emitter.io:443";
    // mqttConfig.uri = "mqtt://api.emitter.io:8080";
    mqttConfig.event_handle = mqttEventHandler;
    auto c = esp_mqtt_client_init(&mqttConfig);
    esp_mqtt_client_start(c); // TODO: check error
    client = c;
    xTaskCreate(
        mqttWorker,
        "mqttWorker",
        4 * 1024,
        nullptr,
        CONFIG_MDF_TASK_DEFAULT_PRIOTY,
        nullptr);
}

void processMQTTMessage(esp_mqtt_event_handle_t event)
{
    printf("processing mqtt message");
    printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
    printf("DATA=%.*s\r\n", event->data_len, event->data);

    cJSON* jsonObj = cJSON_Parse(event->data);
    if (!jsonObj) {
        ESP_LOGE(
            LOG_TAG,
            "Unable to parse json from mqtt message: %s",
            event->data);
        return;
    }
    defer([jsonObj]() {
        cJSON_Delete(jsonObj);
    });

    cJSON* destAddrJson = cJSON_GetObjectItem(jsonObj, "addr");
    if (!destAddrJson) {
        ESP_LOGE(
            LOG_TAG,
            "Unable to parse json from mqtt message: %s",
            event->data);
        return;
    }

    uint8_t destAddr[MWIFI_ADDR_LEN];

    if (!macStrToBinary(destAddrJson->valuestring, destAddr)) {
        ESP_LOGE(
            LOG_TAG,
            "Unable to convert mac string to binary: %s",
            destAddrJson->valuestring);
        return;
    }

    mwifi_data_type_t dataType = {};

    auto returnCode = mwifi_root_write(
        destAddr,
        1,
        &dataType,
        event->data,
        strlen(event->data),
        true);
    if (returnCode != MDF_OK) {
        ESP_LOGE(
            LOG_TAG,
            "Unable to write data to node. node %s, data %s",
            destAddrJson->valuestring, event->data);
        return;
    }
}

esp_err_t mqttEventHandler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED: {
        isClientReady = true;
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_CONNECTED");
        int msgId = esp_mqtt_client_subscribe(
            client,
            CONFIG_MQTT_TOPIC_PREFIX "/*",
            0);
        ESP_LOGI(LOG_TAG, "sent subscribe successful, msgId = %d", msgId);
    } break;
    case MQTT_EVENT_DISCONNECTED:
        isClientReady = false;
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED: {
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        int msgId = esp_mqtt_client_publish(
            client,
            CONFIG_MQTT_TOPIC_PREFIX "/",
            "event subscribed",
            0,
            0,
            0);
        ESP_LOGI(LOG_TAG, "sent publish successful, msg_id=%d", msgId);
    } break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_DATA");
        processMQTTMessage(event);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_ERROR");
        break;
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_BEFORE_CONNECT");
        break;
    }

    return ESP_OK;
}

[[noreturn]] void mqttWorker(void*)
{
    while (true) {
        if (client == nullptr || !isClientReady) {
            vTaskDelay(1000 / portTICK_RATE_MS);
            continue;
        }
        if (messagesQueue.empty()) {
            vTaskDelay(100 / portTICK_RATE_MS);
            continue;
        }
        auto item = messagesQueue.front();
        messagesQueue.pop();

        int msgId = esp_mqtt_client_publish(
            client,
            (std::string(CONFIG_MQTT_TOPIC_PREFIX) + item.first).c_str(),
            item.second.c_str(),
            0,
            0,
            0);

        ESP_LOGI(LOG_TAG, "sent publish successful, msg_id=%d", msgId);

        vTaskDelay(1000 / portTICK_RATE_MS);
    }

    vTaskDelete(nullptr);
}

void mqttPushMessage(const std::string& routingKey, const std::string& jsonMessage)
{
    while (messagesQueue.size() >= size_t(CONFIG_MQTT_MAX_QUEUE_SIZE)) {
        vTaskDelay(100 / portTICK_RATE_MS);
    }
    messagesQueue.push(std::make_pair(routingKey, jsonMessage));
}
