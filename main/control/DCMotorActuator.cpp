#include "control/DCMotorActuator.h"


#include <esp_attr.h>

#include "util/number_utils.h"

#include "driver/ledc.h"
#include "constants/module.h"

#include "AngleControlMessageBuilder.h"

#define LOW_DUTY 200
#define HIGH_DUTY 1000
#define FWD_CHANNEL LEDC_CHANNEL_0
#define REV_CHANNEL LEDC_CHANNEL_1
#define DEADZONE 0.1
#define KP 0.07
#define KI 0.0075
#define KD 0.065
#define MIN_PWM_DUTY 675
#define MAX_PWM_DUTY 1024

DCMotorActuator::DCMotorActuator() {
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 50000, // 4kHz
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t fwd_ledc_channel = {
        .gpio_num = DC_MOTOR_PWM_FWD,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = FWD_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&fwd_ledc_channel));

    ledc_channel_config_t rev_ledc_channel = {
            .gpio_num = DC_MOTOR_PWM_REV,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = REV_CHANNEL,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
            .hpoint = 0,
        };

    ESP_ERROR_CHECK(ledc_channel_config(&rev_ledc_channel));

    setup_encoder();

    this->m_pid_task = nullptr;
    this->m_target_angle = 0;
    this->m_integral = 0;
    this->m_last_error = 0;

    xTaskCreate(reinterpret_cast<TaskFunction_t>(pid_task), "pid_task", 3072, this, 1, &this->m_pid_task);
}

DCMotorActuator::~DCMotorActuator() {
    vTaskDelete(m_pid_task);
}

volatile int32_t encoder_ticks = 0;
volatile int8_t direction = 0;

static void IRAM_ATTR encoder_isr_handler(void* arg) {
    const int a = gpio_get_level(static_cast<gpio_num_t>(DC_ENCODER_A));
    const int b = gpio_get_level(static_cast<gpio_num_t>(DC_ENCODER_B));

    // Determine direction
    if (a == b) {
        encoder_ticks++;
        direction = 1;
    } else {
        encoder_ticks--;
        direction = -1;
    }
}

void DCMotorActuator::setup_encoder() {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << DC_ENCODER_A) | (1ULL << DC_ENCODER_B);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(static_cast<gpio_num_t>(DC_ENCODER_A), encoder_isr_handler, nullptr);
}

void DCMotorActuator::actuate(uint8_t *cmd) {
    const auto* angleControlCmd = Flatbuffers::AngleControlMessageBuilder::parse_angle_control_message(cmd);
    this->m_target_angle = angleControlCmd->angle();
}

void DCMotorActuator::pid_task(char* args) {
    const auto that = reinterpret_cast<DCMotorActuator*>(args);

    while (true) {
        const double degrees = (encoder_ticks * 360.0) / (298.0 * 16);

        const double error = degrees - that->m_target_angle;
        that->m_integral += error * KI;
        const double detivative = (error - that->m_last_error) * KD;
        that->m_last_error = error;

        double control = error * KP + that->m_integral + detivative;
        if (control > 1) {
            control = 1;
        } else if (control < -1) {
            control = -1;
        }
        const auto pwm = util::mapRange<double>(std::abs(control), 0, 1, MIN_PWM_DUTY, MAX_PWM_DUTY);

        if (std::abs(control) < DEADZONE) {
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, REV_CHANNEL, 0));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, REV_CHANNEL));

            ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, FWD_CHANNEL, 0));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, FWD_CHANNEL));
            vTaskDelay(300 / portTICK_PERIOD_MS);
            continue;
        }

        if (control > 0) {
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, FWD_CHANNEL, 0));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, FWD_CHANNEL));

            ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, REV_CHANNEL, pwm));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, REV_CHANNEL));
        } else {
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, REV_CHANNEL, 0));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, REV_CHANNEL));

            ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, FWD_CHANNEL, pwm));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, FWD_CHANNEL));
        }

        vTaskDelay(75 / portTICK_PERIOD_MS);
    }
}
