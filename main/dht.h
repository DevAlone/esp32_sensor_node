#pragma once

#include "driver/rmt.h"
#include <ctime>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <soc/rmt_reg.h>

namespace sensors {
namespace dht {

    enum class DHTReadStatus : unsigned char {
        OK = 0,
        BAD_ARGUMENT = 1,
        WRONG_CHECKSUM = 2,
        NOT_ENOUGH_PULSES = 3,
        UNABLE_TO_INITIALIZE_RINGBUFFER = 4,
        UNABLE_TO_READ_RINGBUFFER = 5,
    };

    class DHTData {
    public:
        int getHumidity();
        float getFloatTemperatureCelcius();
        int getRawTemperatureCelcius();

    private:
        int rawTemperature;
        int rawHumidity;

        friend class DHT;
        friend DHTReadStatus read(gpio_num_t pin, DHTData* outResult);
    };

    // Provides simple API to access DHT sensor
    // only DHT11 is supported yet
    // uses memory to store cached data to not call sensor too frequently
    class DHT {
    public:
        DHT(const gpio_num_t pin)
            : pin(pin)
        {
        }

        DHTReadStatus read(DHTData* outResult);

        const gpio_num_t pin;
    };

    // Reads data from sensor attached to pin
    // do not call more than 1 time in 2 seconds
    DHTReadStatus read(gpio_num_t pin, DHTData* outResult);

}
}
