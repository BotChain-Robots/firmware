//
// Created by Johnathon Slightham on 2025-07-15.
//

// 180 deg servo

#ifndef SERVO1ACTUATOR_H
#define SERVO1ACTUATOR_H

#include <cstdint>
#include "IActuator.h"
#include "ISensor.h"

class Servo1Actuator final : public IActuator {
public:
    Servo1Actuator();
    void actuate(std::uint8_t *cmd) override;
    std::vector<Flatbuffers::SensorValueInstance> get_sensor_data() override;
private:
    uint16_t m_target = 90;
};

#endif //SERVO1ACTUATOR_H
