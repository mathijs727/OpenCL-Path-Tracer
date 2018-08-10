#include "timer.h"

Timer::Timer()
    : m_start(Clock::now())
{
}

template <>
float Timer::elapsed<float>() const
{
    auto diff = Clock::now() - m_start;
    auto durationMs = std::chrono::duration_cast<MicroSeconds>(diff);
    return static_cast<float>(durationMs.count()) / 1000000.0f;
}

template <>
double Timer::elapsed<double>() const
{
    auto diff = Clock::now() - m_start;
    auto durationMs = std::chrono::duration_cast<MicroSeconds>(diff);
    return static_cast<double>(durationMs.count()) / 1000000.0;
}

void Timer::reset()
{
    m_start = Clock::now();
}
