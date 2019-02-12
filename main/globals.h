#pragma once

#include "LedBlinker.h"

static LedBlinker onBoardLed(gpio_num_t(CONFIG_LED_GPIO_NUM));
