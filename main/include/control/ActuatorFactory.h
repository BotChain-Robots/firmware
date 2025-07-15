//
// Created by Johnathon Slightham on 2025-07-15.
//

#ifndef ACTUATORFACTORY_H
#define ACTUATORFACTORY_H

#include "IActuator.h"
#include "flatbuffers_generated/RobotModule_generated.h"

class ActuatorFactory {
public:
    static std::unique_ptr<IActuator> create_actuator(ModuleType type);
};



#endif //ACTUATORFACTORY_H
