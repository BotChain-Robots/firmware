//
// Created by Johnathon Slightham on 2025-10-16.
//

#ifndef ISENSOR_H
#define ISENSOR_H

class ISensor {
public:
    virtual ~ISensor() {}
    virtual void get_reading() = 0; // todo: return a flatbuffer object
};

#endif //ISENSOR_H
