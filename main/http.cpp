#include "http.h"

#include "Defer.h"

static const char* TAG = "esp32_sensor_node";

int sendHttpPost(const std::__cxx11::string& url, const std::__cxx11::string& postData)
{
    esp_http_client_config_t httpConfig = {};
    httpConfig.url = url.c_str();
    httpConfig.method = HTTP_METHOD_POST;
    httpConfig.event_handler = [](esp_http_client_event_t* event) -> esp_err_t {
        switch (event->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
            printf("%.*s", event->data_len, (char*)event->data);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", event->data_len);
            if (!esp_http_client_is_chunked_response(event->client)) {
                printf("%.*s", event->data_len, (char*)event->data);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        }
        return ESP_OK;
    };
    esp_http_client_handle_t httpClient = esp_http_client_init(&httpConfig);
    defer([httpClient] {
        esp_http_client_cleanup(httpClient);
    });

    auto res = esp_http_client_set_post_field(httpClient, postData.c_str(), int(postData.size()));
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "error during settings post field %d", res);
    }
    res = esp_http_client_perform(httpClient);
    return res;

    if (res != ESP_OK) {
        ESP_LOGE(TAG, "error during performing request %d", res);
        return -1;
    } else {
        return esp_http_client_get_status_code(httpClient);
    }
}
