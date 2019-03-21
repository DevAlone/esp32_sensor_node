#include "sensor_node.h"

#include "MQTTClient.h"
#include "globals.h"
#include "helpers.h"
#include "process_sensors.h"

#include "mdf_common.h"
#include "mwifi.h"
#include <string>

static const char* TAG = "esp32_sensor_node";

void rootWriteWorker(void*)
{
    ESP_LOGI(TAG, "rootWriteWorker!");

#if CONFIG_USE_MESH == true
    ESP_LOGI(TAG, "rootWriteWorker:mesh");
    MDF_LOGI("root write worker is running");
    char* data = static_cast<char*>(MDF_CALLOC(1, MWIFI_PAYLOAD_LEN));

    while (mwifi_is_connected()) {
        size_t size = MWIFI_PAYLOAD_LEN;
        uint8_t srcAddr[MWIFI_ADDR_LEN] = {};
        mwifi_data_type_t dataType = {};

        // get message from other node,
        // will wait till message is available
        auto returnCode = mwifi_root_read(
            srcAddr,
            &dataType,
            data,
            &size,
            portMAX_DELAY);

        MDF_ERROR_CONTINUE(returnCode != MDF_OK, "<%s> mwifi_root_read", mdf_err_to_name(returnCode));

        // terminate string
        if (size >= MWIFI_PAYLOAD_LEN) {
            size = MWIFI_PAYLOAD_LEN - 1;
        }
        data[size] = 0;

        std::string jsonData = R"({"addr":")" + macBinaryToStr(srcAddr) + R"(","data":)";
        jsonData += data;
        jsonData += "}";

        // TODO: parse data and detect type
        for (const auto& client : MQTTClient::mqttClients) {
            client->mqttPushMessage("sensor_data/temperature", jsonData);
        }

        if (MQTTClient::mqttClients.size() == 0) {
            MDF_LOGE("mqttClients is empty");
        }

        // TODO: fix
        std::string pingMessage = R"({"type":"ping"})";

        returnCode = mwifi_write(
            srcAddr,
            &dataType,
            pingMessage.c_str(),
            pingMessage.size(),
            false);

        MDF_ERROR_CONTINUE(returnCode != MDF_OK, "<%s> mwifi_write", mdf_err_to_name(returnCode));
    }

    MDF_LOGI("root write worker is exiting");

    MDF_FREE(data);
    vTaskDelete(nullptr);
#else
    ESP_LOGI(TAG, "rootWriteWorker:star");

    size_t packetNumber = 0;

    while (true) {
        onBoardLed.blink(100);

        ++packetNumber;
        ESP_LOGI(TAG, "rootWriteWorker:loop");
        uint8_t srcAddr[MWIFI_ADDR_LEN] = {};

        esp_wifi_get_mac(ESP_IF_WIFI_STA, srcAddr);

        std::string jsonData = R"({"addr":")" + macBinaryToStr(srcAddr) + R"(","data":)";
        jsonData += R"({"packet_number": )"
            + std::to_string(packetNumber)
            + R"(, "type": "sensors_data",)"
            + R"("data": )"
            + getSensorsDataJSON()
            + "}";
        jsonData += "}";

        // TODO: parse data and detect type for routing key
        for (const auto& client : MQTTClient::mqttClients) {
            client->mqttPushMessage("sensors_data", jsonData);
        }

        if (MQTTClient::mqttClients.size() == 0) {
            ESP_LOGI(TAG, "mqttClients is empty");
        }

        ESP_LOGI(TAG, "rootWriteWorker:before delay");
        vTaskDelay(2000 / portTICK_RATE_MS);
        ESP_LOGI(TAG, "rootWriteWorker:after delay");
    }
    ESP_LOGI(TAG, "rootWriteWorker:after loop");
#endif
}

void nodeReadWorker(void*)
{
    char* data = static_cast<char*>(MDF_MALLOC(MWIFI_PAYLOAD_LEN));

    MDF_LOGI("Note read task is running");

    for (;;) {
        if (!mwifi_is_connected()) {
            vTaskDelay(500 / portTICK_RATE_MS);
            continue;
        }

        mwifi_data_type_t dataType = {};
        uint8_t srcAddr[MWIFI_ADDR_LEN] = {};
        size_t size = MWIFI_PAYLOAD_LEN;
        memset(data, 0, MWIFI_PAYLOAD_LEN);
        auto returnCode = mwifi_read(
            srcAddr,
            &dataType,
            data,
            &size,
            portMAX_DELAY);

        MDF_ERROR_CONTINUE(returnCode != MDF_OK, "<%s> mwifi_read", mdf_err_to_name(returnCode));
        MDF_LOGD("Node receive: " MACSTR ", size: %u, data: %s", MAC2STR(srcAddr), size, data);

        auto pJson = cJSON_Parse(data);
        MDF_ERROR_CONTINUE(!pJson, "cJSON_Parse, data format error, data: %s", data);

        auto packetType = cJSON_GetObjectItem(pJson, "type");

        if (!packetType) {
            MDF_LOGW("cJSON_GetObjectItem, packet type is not set");
            cJSON_Delete(pJson);
            continue;
        }
        if (strcmp(packetType->valuestring, "ping") == 0) {
            onBoardLed.blink(100);
        }

        cJSON_Delete(pJson);
    }

    MDF_LOGW("Note read task is exit");

    MDF_FREE(data);
    vTaskDelete(nullptr);
}

void nodeWriteWorker(void*)
{
    mdf_err_t ret = MDF_OK;
    int count = 0;
    char* data = nullptr;
    mwifi_data_type_t data_type = {};

    MDF_LOGI("NODE task is running");

    for (;;) {
        if (!mwifi_is_connected()) {
            vTaskDelay(500 / portTICK_RATE_MS);
            continue;
        }

        /*
        size = asprintf(
            &data,
            R"({"seq":%d,"layer":%d,"status":%d})",
            count++,
            esp_mesh_get_layer(),
            gpio_get_level(CONFIG_LED_GPIO_NUM));
            */
        auto sensorsData = getSensorsDataJSON();

        size_t size = size_t(asprintf(
            &data,
            R"({"packet_number": %d, "type": "sensors_data", "data": %s})",
            count++,
            sensorsData.c_str()));

        MDF_LOGD("Node send, size: %d, data: %s", size, data);
        ret = mwifi_write(nullptr, &data_type, data, size_t(size), true);

        MDF_FREE(data);
        MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mwifi_write", mdf_err_to_name(ret));

        vTaskDelay(2000 / portTICK_RATE_MS);
    }

    MDF_LOGW("NODE task is exit");

    vTaskDelete(nullptr);
}

void printSystemInfo(void*)
{
    uint8_t primary = 0;
    wifi_second_chan_t second = wifi_second_chan_t(0);
    mesh_addr_t parent_bssid = {};
    uint8_t sta_mac[MWIFI_ADDR_LEN] = {};
    mesh_assoc_t mesh_assoc = {};
    wifi_sta_list_t wifi_sta_list = {};

    esp_wifi_get_mac(ESP_IF_WIFI_STA, sta_mac);
    esp_wifi_ap_get_sta_list(&wifi_sta_list);
    esp_wifi_get_channel(&primary, &second);
    esp_wifi_vnd_mesh_get(&mesh_assoc);
    esp_mesh_get_parent_bssid(&parent_bssid);

    MDF_LOGI("System information, channel: %d, layer: %d, self mac: " MACSTR ", parent bssid: " MACSTR
             ", parent rssi: %d, node num: %d, free heap: %u",
        primary,
        esp_mesh_get_layer(), MAC2STR(sta_mac), MAC2STR(parent_bssid.addr),
        mesh_assoc.rssi, esp_mesh_get_total_node_num(), esp_get_free_heap_size());

    for (int i = 0; i < wifi_sta_list.num; i++) {
        MDF_LOGI("Child mac: " MACSTR, MAC2STR(wifi_sta_list.sta[i].mac));
    }

    uint32_t freeMemoryBytes = esp_get_free_heap_size();
    MDF_LOGI(
        "Free memory: %ud bytes, %ud kbytes, %ud mbytes",
        freeMemoryBytes,
        freeMemoryBytes / 1024,
        freeMemoryBytes / 1024 / 1024);

#ifdef MEMORY_DEBUG
    if (!heap_caps_check_integrity_all(true)) {
        MDF_LOGE("At least one heap is corrupt");
    }

    mdf_mem_print_heap();
    mdf_mem_print_record();
#endif /**< MEMORY_DEBUG */
}
