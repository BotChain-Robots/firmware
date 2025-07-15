//
// Created by Johnathon Slightham on 2025-07-15.
//

#include "control/ActuatorFactory.h"

#include <control/Servo1Actuator.h>

std::unique_ptr<IActuator> ActuatorFactory::create_actuator(ModuleType type) {
    switch (type) {
        case ModuleType_SERVO_1:
            return std::make_unique<Servo1Actuator>();
        default:
            return nullptr;
    }
}
