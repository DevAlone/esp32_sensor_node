#pragma once

#include "mwifi.h"

#include <cstdint>
#include <string>

bool macStrToBinary(const char* macStr, uint8_t outArray[MWIFI_ADDR_LEN]);
std::string macBinaryToStr(uint8_t addr[MWIFI_ADDR_LEN]);

long long millis();
