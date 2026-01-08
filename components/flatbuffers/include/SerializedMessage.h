//
// Created by Johnathon Slightham on 2025-07-05.
//

#ifndef SERIALIZEDMESSAGE_H
#define SERIALIZEDMESSAGE_H

#include <cstdint>

namespace Flatbuffers {
    struct SerializedMessage {
        void* data;
        std::size_t size;
    };
}

#endif //SERIALIZEDMESSAGE_H
