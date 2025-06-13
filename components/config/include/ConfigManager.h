//
// Created by Johnathon Slightham on 2025-06-12.
//

#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include "constants/config.h"

class ConfigManager {
public:
    static void init_config();

    static uint16_t get_module_id();
    static ModuleType get_module_type();

    static void set_module_id(uint16_t id);
    static void set_module_type(ModuleType type);
};

#endif //CONFIGMANAGER_H
