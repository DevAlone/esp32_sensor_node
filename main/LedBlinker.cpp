#include "LedBlinker.h"

#include <chrono>

LedBlinker::LedBlinker(const gpio_num_t pin)
    : pin(pin)
{
    xTaskCreate(
        LedBlinker::task,
        "LedBlinker::task",
        4 * 1024,
        this,
        CONFIG_MDF_TASK_DEFAULT_PRIOTY,
        nullptr);
}

void LedBlinker::blink(size_t durationMs)
{
    gpio_set_level(pin, 1);
    using namespace std::chrono;
    disableTimeMs = duration_cast<milliseconds>(
                        system_clock::now().time_since_epoch())
                        .count()
        + static_cast<long long>(durationMs);
}

void LedBlinker::task(void* obj)
{
    auto self = static_cast<LedBlinker*>(obj);

    while (true) {
        using namespace std::chrono;
        auto currentTimeMs = duration_cast<milliseconds>(
            system_clock::now().time_since_epoch())
                                 .count();

        if (currentTimeMs >= self->disableTimeMs) {
            gpio_set_level(self->pin, 0);
        }

        vTaskDelay(10 / portTICK_RATE_MS);
    }
}
