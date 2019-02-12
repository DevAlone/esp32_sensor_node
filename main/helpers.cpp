#include "helpers.h"

#include "mwifi.h"

#include <chrono>

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

long long millis()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch())
        .count();
}
