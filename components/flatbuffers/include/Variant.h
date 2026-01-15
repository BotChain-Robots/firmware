#ifndef VARIANT_H
#define VARIANT_H

#include <variant> // NOLINT

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};

template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

#endif // VARIANT_H
