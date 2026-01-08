//
// Created by Johnathon Slightham on 2025-07-15.
//

#include "control/Servo1Actuator.h"
#include "AngleControlMessageBuilder.h"
#include "constants/module.h"
#include "driver/ledc.h"
#include "flatbuffers_generated/SensorMessage_generated.h"
#include "util/number_utils.h"
#include "SensorMessageBuilder.h"

#define LOW_DUTY 200
#define HIGH_DUTY 1000
#define PWM_FREQ 50 // 4khz

Servo1Actuator::Servo1Actuator() {
  ledc_timer_config_t ledc_timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = LEDC_TIMER_13_BIT,
      .timer_num = LEDC_TIMER_0,
      .freq_hz = PWM_FREQ,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

  ledc_channel_config_t ledc_channel = {
      .gpio_num = SERVO_GPIO,
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = LEDC_CHANNEL_0,
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = LEDC_TIMER_0,
      .duty = (HIGH_DUTY + LOW_DUTY) / 2, // move motor to midpoint initially
      .hpoint = 0,
  };

  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void Servo1Actuator::actuate(uint8_t *cmd) {
  const auto *angleControlCmd =
      Flatbuffers::AngleControlMessageBuilder::parse_angle_control_message(cmd);
  const auto newDuty = util::mapRange<int32_t>(angleControlCmd->angle(), 0, 180,
                                               LOW_DUTY, HIGH_DUTY);

  m_target = angleControlCmd->angle();
  std::cout << "actuating to " << angleControlCmd->angle() << std::endl;

  ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, newDuty));
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
}

std::vector<Flatbuffers::SensorValueInstance> Servo1Actuator::get_sensor_data() {
    return {{m_target}};
}
