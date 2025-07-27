//
// Created by Johnathon Slightham on 2025-07-26.
//

#include <constants/module.h>
#include "OrientationDetection.h"

#include <ConfigManager.h>
#include <iostream>
#include <bits/ostream.tcc>

void OrientationDetection::init() {
    for (int i = 0; i < MODULE_TO_NUM_CHANNELS_MAP[ConfigManager::get_module_type()]; i++) {
        setup_gpio(static_cast<gpio_num_t>(CHANNEL_TO_0_DEG_MAP[i]));
        setup_gpio(static_cast<gpio_num_t>(CHANNEL_TO_90_DEG_MAP[i]));
        setup_gpio(static_cast<gpio_num_t>(CHANNEL_TO_180_DEG_MAP[i]));
        setup_gpio(static_cast<gpio_num_t>(CHANNEL_TO_270_DEG_MAP[i]));
    }
}

Orientation OrientationDetection::get_orientation(const uint8_t channel) {
    if (gpio_get_level(static_cast<gpio_num_t>(CHANNEL_TO_90_DEG_MAP[channel]))) {
        std::cout << "90deg" << std::endl;
        return Orientation_Deg90;
    } else if (gpio_get_level(static_cast<gpio_num_t>(CHANNEL_TO_180_DEG_MAP[channel]))) {
        std::cout << "180deg" << std::endl;
        return Orientation_Deg180;
    } else if (gpio_get_level(static_cast<gpio_num_t>(CHANNEL_TO_270_DEG_MAP[channel]))) {
        std::cout << "270deg" << std::endl;
        return Orientation_Deg270;
    } else {
        std::cout << "No orientation detected" << std::endl;
        return Orientation_Deg0;
    }
}

void OrientationDetection::setup_gpio(const gpio_num_t pin) {
    const gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
}
