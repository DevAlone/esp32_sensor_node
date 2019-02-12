#pragma once

#include "mqtt.h"
#include "mqtt_client.h"

#include <cstring>
#include <string>

void mqttInit();
esp_err_t mqttEventHandler(esp_mqtt_event_handle_t event);
[[noreturn]] void mqttWorker(void*);

void mqttPushMessage(
    const std::string& routingKey,
    const std::string& jsonMessage);
