
#ifndef SENSORMESSAGEBUILDER_H
#define SENSORMESSAGEBUILDER_H

#include "SerializedMessage.h"
#include "flatbuffers_generated/SensorMessage_generated.h"

namespace Flatbuffers {
    struct SensorValueInstance {
        uint16_t angle; // todo: change to a variant
    };

    class SensorMessageBuilder{
    public:
        SensorMessageBuilder() : builder_(128) {}

        SerializedMessage build_sensor_message(std::vector<SensorValueInstance>& values);

    private:
        flatbuffers::FlatBufferBuilder builder_;
    };
}

#endif //SENSORMESSAGEBUILDER_H
