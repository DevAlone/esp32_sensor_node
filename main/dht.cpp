#include "dht.h"

#include "driver/rmt.h"
#include <chrono>
#include <ctime>
#include <driver/rmt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <soc/rmt_reg.h>

namespace sensors {
namespace dht {

    void dht11_rmt_rx_init(gpio_num_t gpio_pin, rmt_channel_t channel);
    DHTReadStatus parse_items(
        rmt_item32_t* item, int item_num, int* humidity, int* temp_x10);
    DHTReadStatus dht11_rmt_rx(
        gpio_num_t gpio_pin,
        rmt_channel_t rmt_channel,
        int* humidity,
        int* temp_x10);

    int DHTData::getRawTemperatureCelcius()
    {
        return rawTemperature;
    }

    int DHTData::getHumidity()
    {
        return rawHumidity;
    }

    float DHTData::getFloatTemperatureCelcius()
    {
        return float(rawTemperature) / 10.0f;
    }

    DHTReadStatus DHT::read(DHTData* outResult)
    {
        // TODO: limit number of requests

        if (outResult == nullptr) {
            return DHTReadStatus::BAD_ARGUMENT;
        }

        return dht::read(pin, outResult);
    }

    DHTReadStatus read(gpio_num_t pin, DHTData* outResult)
    {
        if (!outResult) {
            return DHTReadStatus::BAD_ARGUMENT;
        }

        const rmt_channel_t rmt_channel = rmt_channel_t(0);

        dht11_rmt_rx_init(pin, rmt_channel);

        auto status = dht11_rmt_rx(pin, rmt_channel, &outResult->rawHumidity, &outResult->rawTemperature);
        rmt_driver_uninstall(rmt_channel);

        return status;
    }

    // RMT receiver initialization
    void dht11_rmt_rx_init(gpio_num_t gpio_pin, rmt_channel_t channel)
    {
        const int RMT_CLK_DIV = 80; /*!< RMT counter clock divider */
        const int RMT_TICK_10_US = (80000000 / RMT_CLK_DIV / 100000); /*!< RMT counter value for 10 us.(Source clock is APB clock) */
        const int rmt_item32_tIMEOUT_US = 1000; /*!< RMT receiver timeout value(us) */

        rmt_config_t rmt_rx;
        rmt_rx.gpio_num = gpio_pin;
        rmt_rx.channel = channel;
        rmt_rx.clk_div = RMT_CLK_DIV;
        rmt_rx.mem_block_num = 1;
        rmt_rx.rmt_mode = RMT_MODE_RX;
        rmt_rx.rx_config.filter_en = false;
        rmt_rx.rx_config.filter_ticks_thresh = 100;
        rmt_rx.rx_config.idle_threshold = rmt_item32_tIMEOUT_US / 10 * (RMT_TICK_10_US);
        rmt_config(&rmt_rx);
        rmt_driver_install(rmt_rx.channel, 1000, 0);
    }

    // Processing the pulse data into temp and humidity
    DHTReadStatus parse_items(rmt_item32_t* item, int item_num, int* humidity, int* temp_x10)
    {
        int i = 0;
        unsigned rh = 0, temp = 0, checksum = 0;

        // Check we have enough pulses
        if (item_num < 42) {
            return DHTReadStatus::NOT_ENOUGH_PULSES;
        }

        // Skip the start of transmission pulse
        item++;

        // Extract the humidity data
        for (i = 0; i < 16; i++, item++) {
            rh = (rh << 1) + (item->duration1 < 35 ? 0 : 1);
        }

        // Extract the temperature data
        for (i = 0; i < 16; i++, item++) {
            temp = (temp << 1) + (item->duration1 < 35 ? 0 : 1);
        }

        // Extract the checksum
        for (i = 0; i < 8; i++, item++) {
            checksum = (checksum << 1) + (item->duration1 < 35 ? 0 : 1);
        }

        // Check the checksum
        if ((((temp >> 8) + temp + (rh >> 8) + rh) & 0xFF) != checksum) {
            printf("dht11 Checksum failure %4X %4X %2X\n", temp, rh, checksum);
            return DHTReadStatus::WRONG_CHECKSUM;
        }

        // Store into return values
        *humidity = rh >> 8;
        *temp_x10 = (temp >> 8) * 10 + (temp & 0xFF);
        return DHTReadStatus::OK;
    }

    // Use the RMT receiver to get the DHT data
    DHTReadStatus dht11_rmt_rx(
        gpio_num_t gpio_pin,
        rmt_channel_t rmt_channel,
        int* humidity,
        int* temp_x10)
    {
        RingbufHandle_t rb = nullptr;
        size_t rx_size = 0;
        rmt_item32_t* item;

        // get RMT RX ringbuffer
        rmt_get_ringbuf_handle(rmt_channel, &rb);
        if (!rb) {
            return DHTReadStatus::UNABLE_TO_INITIALIZE_RINGBUFFER;
        }

        // Send the 20ms pulse to kick the DHT into life
        gpio_set_level(gpio_pin, 1);
        gpio_set_direction(gpio_pin, GPIO_MODE_OUTPUT);
        ets_delay_us(1000);
        gpio_set_level(gpio_pin, 0);
        ets_delay_us(20000);

        // Bring rmt_rx_start & rmt_rx_stop into cache
        rmt_rx_start(rmt_channel, 1);
        rmt_rx_stop(rmt_channel);

        // Now get the sensor to send the data
        gpio_set_level(gpio_pin, 1);
        gpio_set_direction(gpio_pin, GPIO_MODE_INPUT);

        // Start the RMT receiver for the data this time
        rmt_rx_start(rmt_channel, 1);

        // Pull the data from the ring buffer
        item = static_cast<rmt_item32_t*>(xRingbufferReceive(rb, &rx_size, 2));
        if (item == nullptr) {
            return DHTReadStatus::UNABLE_TO_READ_RINGBUFFER;
        }

        size_t n;
        n = rx_size / 4;
        // parse data value from ringbuffer.
        auto status = parse_items(item, int(n), humidity, temp_x10);
        // after parsing the data, return spaces to ringbuffer.
        vRingbufferReturnItem(rb, static_cast<void*>(item));

        // Stop the RMT Receiver
        rmt_rx_stop(rmt_channel);

        return status;
    }

}
}
