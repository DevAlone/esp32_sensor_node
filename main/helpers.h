#pragma once

#include "mwifi.h"

#include <cstdint>

bool macStrToBinary(const char* macStr, uint8_t outArray[MWIFI_ADDR_LEN]);

long long millis();
