//
// Created by Johnathon Slightham on 2025-07-15.
//

#ifndef INT_UTILS_H
#define INT_UTILS_H

#include <iostream>
#include <type_traits>

namespace util {
    template<typename T>
    T mapRange(T value, T inMin, T inMax, T outMin, T outMax) {
        static_assert(std::is_arithmetic<T>::value, "Template parameter must be a numeric type");

        if (inMin == inMax) {
            return value;
        }

        return (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
    }
}

#endif //INT_UTILS_H
