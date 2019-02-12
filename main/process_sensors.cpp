#include "process_sensors.h"

#include "helpers.h"

#include "sdkconfig.h"

#if (CONFIG_SENSOR_DHT11_ENABLED == true)
#include "dht.h"
using namespace sensors;
#endif
#include "mdf_common.h"

#if (CONFIG_SENSOR_DHT11_ENABLED == true)
static dht::DHT dht11Sensor(gpio_num_t(CONFIG_SENSOR_DHT11_PIN));
#endif

static const char* LOG_TAG = "process_sensors";

namespace std {
std::string to_string(const std::string& value)
{
    return value;
}
}

template <typename T>
std::string makeSensorDataString(
    const std::string& type,
    gpio_num_t pin,
    long long timestampMs,
    const T& value)
{
    std::string result = "{";
    result += std::string(R"("status":"ok",)");
    result += std::string(R"("type":")") + type + "\",";
    result += std::string(R"("pin":)") + std::to_string(pin) + ",";
    result += std::string(R"("timestamp_ms":)") + std::to_string(timestampMs) + ",";
    result += std::string(R"("value":)") + std::to_string(value);
    result += "}";
    return result;
}

std::string makeSensorDataErrorString(
    const std::string& errorMessage, int errorCode)
{
    std::string result = "{";
    result += std::string(R"("status":"error",)");
    result += std::string(R"("error_message":")") + errorMessage + "\",";
    result += std::string(R"("error_code":")") + std::to_string(errorCode) + "\"";
    result += "}";
    return result;
}

std::string getSensorsDataJSON()
{
    std::string result = "[";

#if (CONFIG_SENSOR_DHT11_ENABLED == true)
    dht::DHTData dht11Data;
    auto readStatus = dht11Sensor.read(&dht11Data);
    if (readStatus == dht::DHTReadStatus::OK) {
        ESP_LOGI(
            LOG_TAG,
            "temperature: %f, humidity: %d",
            double(dht11Data.getFloatTemperatureCelcius()),
            dht11Data.getHumidity());
        std::string value = "{";
        value += R"("temperature_c":)" + std::to_string(dht11Data.getFloatTemperatureCelcius()) + ",";
        value += R"("humidity":)" + std::to_string(dht11Data.getHumidity()) + "";
        value += "}";
        result += makeSensorDataString(
            "dht11",
            dht11Sensor.pin,
            millis(),
            value);
    } else {
        ESP_LOGI(
            LOG_TAG,
            "failed to read data from sensor: %d",
            int(readStatus));
        result += makeSensorDataErrorString(
            "failed to read data from sensor",
            int(readStatus));
    }
#endif

    result += "]";

    return result;
}
