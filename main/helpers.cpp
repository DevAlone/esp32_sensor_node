#include "helpers.h"

#include "mwifi.h"

#include <chrono>

static const char* TAG = "esp32_sensor_node/helpers";

bool macStrToBinary(const char* macStr, uint8_t outArray[MWIFI_ADDR_LEN])
{
    uint32_t macData[MWIFI_ADDR_LEN];

    if (sscanf(
            macStr,
            MACSTR,
            macData,
            macData + 1,
            macData + 2,
            macData + 3,
            macData + 4,
            macData + 5)
        != 6) {
        return false;
    }
    for (size_t i = 0; i < MWIFI_ADDR_LEN; ++i) {
        outArray[i] = uint8_t(macData[i]);
    }

    return true;
}

std::string macBinaryToStr(uint8_t addr[MWIFI_ADDR_LEN])
{
    char* data = nullptr;
    asprintf(&data, MACSTR, MAC2STR(addr));
    std::string result(data);
    MDF_FREE(data);
    return result;
}

long long millis()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch())
        .count();
}
