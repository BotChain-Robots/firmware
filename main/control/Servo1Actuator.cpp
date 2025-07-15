//
// Created by Johnathon Slightham on 2025-07-15.
//

#include "control/Servo1Actuator.h"
#include "util/number_utils.h"

#include "driver/ledc.h"
#include "constants/module.h"

#include "AngleControlMessageBuilder.h"

#define LOW_DUTY 200
#define HIGH_DUTY 1000

Servo1Actuator::Servo1Actuator() {
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 50, // 4kHz
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .gpio_num = SERVO_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 600, // midpoint
        .hpoint = 0,
    };

    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void Servo1Actuator::actuate(uint8_t *cmd) {
    for (int i = 0; i < 512; i++) {
        printf("%x ", cmd[i]);
    }
    printf("\n");
    const auto* angleControlCmd = Flatbuffers::AngleControlMessageBuilder::parse_angle_control_message(cmd);
    std::cout << "cmd: " << angleControlCmd->angle() << std::endl;
    const auto newDuty = util::mapRange<int32_t>(angleControlCmd->angle(), 0, 180, LOW_DUTY, HIGH_DUTY);
    std::cout << "newDuty: " << newDuty << std::endl;

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, newDuty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
}

