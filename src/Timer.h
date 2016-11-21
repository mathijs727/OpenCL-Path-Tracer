#pragma once

#include <chrono>

struct Timer
{
	typedef std::chrono::high_resolution_clock Clock;
	typedef std::chrono::steady_clock::time_point TimePoint;
	typedef std::chrono::microseconds MicroSeconds;

	TimePoint start;
	Timer() : start(get()) {  }
	float elapsed() const {
		auto diff = get() - start;
		auto durationMs = std::chrono::duration_cast<MicroSeconds>(
			diff);
		return static_cast<float>(durationMs.count()) / 1000000.0f;
	}
	static TimePoint get() {
		return Clock::now();
	}

	//static double to_time(const TimePoint vt) {
	//	// TODO...
	//}

	void reset() { start = get(); }
};