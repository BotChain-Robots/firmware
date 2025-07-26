//
// Created by Johnathon Slightham on 2025-07-12.
//

#ifndef MODULE_H
#define MODULE_H

#include <unordered_map>

#include "flatbuffers_generated/RobotModule_generated.h"

inline std::unordered_map<int, int> MODULE_TO_NUM_CHANNELS_MAP {{ModuleType_SPLITTER, 4}, {ModuleType_SERVO_1, 2}, {ModuleType_DC_MOTOR, 1}};

#define PC_ADDR 0

#define MAX_WIRED_CONNECTIONS 4

#define SERVO_GPIO 1

#define DC_MOTOR_PWM_FWD 1
#define DC_MOTOR_PWM_REV 2
#define DC_ENCODER_A 15
#define DC_ENCODER_B 16

#endif //MODULE_H
