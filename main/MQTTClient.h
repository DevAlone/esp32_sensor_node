#pragma once

#include "mqtt_client.h"

#include <cstring>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>

class MQTTClient {
public:
    static std::unordered_set<MQTTClient*> mqttClients;
    static std::unordered_map<esp_mqtt_client_handle_t, MQTTClient*> mqttClientsMap;

    MQTTClient(const char* mqttURI);
    virtual ~MQTTClient();
    void mqttInit();

    [[noreturn]] void mqttWorker();

    void mqttPushMessage(
        const std::string& routingKey,
        const std::string& jsonMessage);

    void processMQTTMessage(esp_mqtt_event_handle_t event);

    static esp_err_t mqttEventHandler(esp_mqtt_event_handle_t event);
    static void mqttStaticWorker(void*);

private:
    esp_mqtt_client_handle_t client = nullptr;
    bool isClientReady = false;
    std::queue<std::pair<std::string, std::string>> messagesQueue;

    // const char* mqttURI = "mqtt://" CONFIG_MQTT_USERNAME ":" CONFIG_MQTT_PASSWORD "@" CONFIG_MQTT_IP ":" CONFIG_MQTT_PORT;
    const char* mqttURI;
};
