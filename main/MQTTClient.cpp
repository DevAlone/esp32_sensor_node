#include "MQTTClient.h"

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

std::unordered_set<MQTTClient*> MQTTClient::mqttClients;
std::unordered_map<esp_mqtt_client_handle_t, MQTTClient*> MQTTClient::mqttClientsMap;

MQTTClient::MQTTClient(const char* mqttURI)
    : mqttURI(mqttURI)
{
    xTaskCreate(
        mqttStaticWorker,
        "mqttStaticWorker",
        4 * 1024,
        this,
        CONFIG_MDF_TASK_DEFAULT_PRIOTY,
        nullptr);
    mqttClients.insert(this);
}

MQTTClient::~MQTTClient()
{
    if (client) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
    }

    mqttClients.erase(this);
    auto it = mqttClientsMap.find(client);
    if (it != mqttClientsMap.end()) {
        mqttClientsMap.erase(it);
    }
}

void MQTTClient::mqttInit()
{
    if (client) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = nullptr;
    }

    esp_mqtt_client_config_t mqttConfig = {};
    mqttConfig.uri = mqttURI;
    // mqttConfig.uri = "mqtts://api.emitter.io:443";
    // mqttConfig.uri = "mqtt://api.emitter.io:8080";
    mqttConfig.event_handle = mqttEventHandler;
    auto c = esp_mqtt_client_init(&mqttConfig);
    client = c;
    mqttClientsMap[client] = this;
    auto err = esp_mqtt_client_start(c); // TODO: check error
    if (err != ESP_OK) {
        ESP_LOGE(LOG_TAG, "mqtt connection error. error: %d", err);
    }
}

void MQTTClient::processMQTTMessage(esp_mqtt_event_handle_t event)
{
#if CONFIG_USE_MESH == true

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
#else
    // TODO: process
#endif
}

esp_err_t MQTTClient::mqttEventHandler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    MQTTClient* mqttClient = nullptr;
    {
        auto it = mqttClientsMap.find(client);
        if (it != mqttClientsMap.end()) {
            mqttClient = it->second;
        }
    }
    const char* mqttURI = mqttClient ? mqttClient->mqttURI : "";

    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED: {
        if (mqttClient) {
            mqttClient->isClientReady = true;
        }
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_CONNECTED, client: %s", mqttURI);
        int msgId = esp_mqtt_client_subscribe(
            client,
            CONFIG_MQTT_TOPIC_PREFIX "/*",
            0);
        ESP_LOGI(LOG_TAG, "sent subscribe successful, msgId = %d", msgId);
    } break;
    case MQTT_EVENT_DISCONNECTED:
        if (mqttClient) {
            mqttClient->isClientReady = false;
        }
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_DISCONNECTED, client: %s", mqttURI);
        break;
    case MQTT_EVENT_SUBSCRIBED: {
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d, client: %s", event->msg_id, mqttURI);
        // TODO: remove
        /*
         * int msgId = esp_mqtt_client_publish(
            client,
            CONFIG_MQTT_TOPIC_PREFIX "/",
            "event subscribed",
            0,
            0,
            0);
        ESP_LOGI(LOG_TAG, "sent publish successful, msg_id=%d", msgId);
            */
    } break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d, client: %s", event->msg_id, mqttURI);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d. client: %s", event->msg_id, mqttURI);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_DATA, client: %s", mqttURI);
        if (mqttClient) {
            mqttClient->processMQTTMessage(event);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(LOG_TAG, "MQTT_EVENT_ERROR. client: %s, data: %s", mqttURI, event->data);
        break;
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(
            LOG_TAG,
            "MQTT_EVENT_BEFORE_CONNECT. Trying to connect to address '%s', client: %lld, mqttClient: %lld",
            mqttURI,
            int64_t(client),
            int64_t(mqttClient));
        break;
    }

    return ESP_OK;
}

void MQTTClient::mqttStaticWorker(void* ptr)
{
    if (ptr == nullptr) {
        ESP_LOGE(LOG_TAG, "mqttStaticWorker():null ptr");
        return;
    }

    static_cast<MQTTClient*>(ptr)->mqttWorker();
}

[[noreturn]] void MQTTClient::mqttWorker()
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
            (std::string(CONFIG_MQTT_TOPIC_PREFIX) + "/" + item.first).c_str(),
            item.second.c_str(),
            0,
            0,
            0);

        ESP_LOGI(LOG_TAG, "sent publish successful, msg_id=%d, client: %s", msgId, mqttURI);

        vTaskDelay(1000 / portTICK_RATE_MS);
    }

    vTaskDelete(nullptr);
}

void MQTTClient::mqttPushMessage(const std::string& routingKey, const std::string& jsonMessage)
{
    while (messagesQueue.size() >= size_t(CONFIG_MQTT_MAX_QUEUE_SIZE)) {
        vTaskDelay(100 / portTICK_RATE_MS);
    }
    messagesQueue.push(std::make_pair(routingKey, jsonMessage));

    ESP_LOGI(LOG_TAG, "Message pushed to queue, size: %u, data: %s, client: %s", jsonMessage.size(), jsonMessage.c_str(), mqttURI);
}
