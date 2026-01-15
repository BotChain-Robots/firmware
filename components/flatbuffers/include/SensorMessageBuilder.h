
#ifndef SENSORMESSAGEBUILDER_H
#define SENSORMESSAGEBUILDER_H

#include "SerializedMessage.h"
#include "flatbuffers_generated/SensorMessage_generated.h"

namespace Flatbuffers {

struct target_angle {
    int16_t angle;
};

struct current_angle {
    int16_t angle;
};

typedef std::variant<target_angle, current_angle> sensor_value;

class SensorMessageBuilder {
  public:
    SensorMessageBuilder() : builder_(128) {
    }

    SerializedMessage build_sensor_message(std::vector<sensor_value> &values);

  private:
    flatbuffers::FlatBufferBuilder builder_;
};
} // namespace Flatbuffers

#endif //SENSORMESSAGEBUILDER_H
