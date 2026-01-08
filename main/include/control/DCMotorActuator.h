// DC motor with quatrature hall encoder

#ifndef DCMOTORACTUATOR_H
#define DCMOTORACTUATOR_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "IActuator.h"

class DCMotorActuator final : public IActuator {
public:
    DCMotorActuator();
    ~DCMotorActuator() override;
    void actuate(uint8_t *cmd) override;
    std::vector<Flatbuffers::SensorValueInstance> get_sensor_data() override;
private:
    void setup_encoder();
    static void pid_task(char* args);

    double m_current_angle;
    int64_t m_target_angle;
    TaskHandle_t m_pid_task;

    double m_integral;
    double m_last_error;
};

#endif //SERVO1ACTUATOR_H
