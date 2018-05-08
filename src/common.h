#pragma once

template<typename T>
struct Pi;

template<>
struct Pi<float> {
    static constexpr float value = 3.1415927f;
};

template<>
struct Pi<double> {
    static constexpr float value = 3.141592653589793;
};
