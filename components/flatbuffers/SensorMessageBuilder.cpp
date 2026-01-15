//
// Created by Johnathon Slightham on 2025-06-30.
//

#include "SensorMessageBuilder.h"
#include "SerializedMessage.h"
#include "Variant.h"
#include "flatbuffers_generated/SensorMessage_generated.h"

namespace Flatbuffers {

SerializedMessage SensorMessageBuilder::build_sensor_message(std::vector<sensor_value> &values) {
    builder_.Clear();

    std::vector<flatbuffers::Offset<void>> values_vec;
    std::vector<uint8_t> sensor_values_vec;

    for (const auto &v : values) {
        std::visit(
            overloaded{
                [&](target_angle a) {
                    values_vec.push_back(Messaging::CreateTargetAngle(builder_, a.angle).Union());
                    sensor_values_vec.push_back(Messaging::SensorValue_TargetAngle);
                },
                [&](current_angle a) {
                    values_vec.push_back(Messaging::CreateCurrentAngle(builder_, a.angle).Union());
                    sensor_values_vec.push_back(Messaging::SensorValue_CurrentAngle);
                },
            },
            v);
    }

    auto values_fb_vec = builder_.CreateVector(values_vec);
    const auto values_type_fb_vec = builder_.CreateVector(sensor_values_vec);

    const auto message =
        Messaging::CreateSensorMessage(builder_, values_type_fb_vec, values_fb_vec);

    builder_.Finish(message);

    return {builder_.GetBufferPointer(), builder_.GetSize()};
}
} // namespace Flatbuffers
