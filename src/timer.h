#pragma once
#include <chrono>

class Timer {
public:
    Timer();

    template <typename T>
    T elapsed() const;

    void reset();

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::high_resolution_clock::time_point;
    using MicroSeconds = std::chrono::microseconds;

    TimePoint m_start;
};
