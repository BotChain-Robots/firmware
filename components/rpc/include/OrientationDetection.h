//
// Created by Johnathon Slightham on 2025-07-26.
//

#ifndef ORIENTATIONDETECTION_H
#define ORIENTATIONDETECTION_H
#include <flatbuffers_generated/RobotModule_generated.h>
#include "driver/gpio.h"

class OrientationDetection {
public:
    static void init();
    static Orientation get_orientation(uint8_t channel);

private:
    static void setup_gpio(gpio_num_t pin);

};

#endif //ORIENTATIONDETECTION_H
