//
// Created by Johnathon Slightham on 2025-06-30.
//

#include "SensorMessageBuilder.h"
#include "SerializedMessage.h"
#include "flatbuffers_generated/SensorMessage_generated.h"

namespace Flatbuffers {

    SerializedMessage SensorMessageBuilder::build_sensor_message(std::vector<SensorValueInstance>& values) {
        builder_.Clear();

        std::vector<flatbuffers::Offset<void>> values_vec;
        std::vector<uint8_t> sensor_values_vec;

        for (const auto& v : values) {
            values_vec.push_back(Messaging::CreateAngle(builder_, v.angle).Union());
            sensor_values_vec.push_back(Messaging::SensorValue_Angle);
        }

        auto values_fb_vec = builder_.CreateVector(values_vec);
        const auto values_type_fb_vec = builder_.CreateVector(sensor_values_vec);

        const auto message = Messaging::CreateSensorMessage(builder_, values_type_fb_vec, values_fb_vec);

        builder_.Finish(message);

        return { builder_.GetBufferPointer(), builder_.GetSize() };
    }
}
