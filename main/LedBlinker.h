#pragma once

#include "mdf_common.h"

class LedBlinker {
public:
    LedBlinker(const gpio_num_t pin);
    void blink(size_t durationMs);

private:
    gpio_num_t pin;
    long long disableTimeMs = 0;

    [[noreturn]] static void task(void* obj);
};
