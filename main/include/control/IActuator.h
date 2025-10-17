//
// Created by Johnathon Slightham on 2025-07-15.
//

#ifndef IACTUATOR_H
#define IACTUATOR_H

class IActuator {
public:
    virtual ~IActuator() {}
    virtual void actuate(uint8_t *cmd) = 0;
};

#endif //IACTUATOR_H
