//
// Created by Johnathon Slightham on 2025-10-16.
//

#ifndef SENSORFACTORY_H
#define SENSORFACTORY_H

class SensorFactory {
public:
    static std::unique_ptr<ISensor> create_sensor(ModuleType type);
};

#endif //SENSORFACTORY_H
