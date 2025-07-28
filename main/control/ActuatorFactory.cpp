//
// Created by Johnathon Slightham on 2025-07-15.
//

#include <memory>

#include "control/ActuatorFactory.h"

#include "control/DCMotorActuator.h"
#include "control/Servo1Actuator.h"

#include "flatbuffers_generated/RobotModule_generated.h"

std::unique_ptr<IActuator> ActuatorFactory::create_actuator(const ModuleType type) {
    switch (type) {
        case ModuleType_SERVO_1:
            return std::make_unique<Servo1Actuator>();
        case ModuleType_SERVO_2:
            return std::make_unique<Servo1Actuator>();
        case ModuleType_DC_MOTOR:
            return std::make_unique<DCMotorActuator>();
        default:
            return nullptr;
    }
}
