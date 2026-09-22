// firmware/include/core/night_window.h
#pragma once
#include <cstdint>

// When the framed arrivals screen should wear its dark palette.
//
// Pure arithmetic on an epoch second rather than a call to localtime(): that
// keeps it testable on the host, and the device only ever has one timezone to
// worry about anyway.

// The feed this device reads is a Singapore bus API, so the device is
// Singapore-only and the offset is a constant rather than a setting.
// Singapore has never observed daylight saving, so there is no second case.
constexpr int32_t kLocalUtcOffsetSeconds = 8 * 3600;

// 19:00 inclusive to 07:00 exclusive, local. The window wraps midnight, which
// is the whole reason this is a function and not a comparison.
constexpr int kNightStartHour = 19;
constexpr int kNightEndHour = 7;

// Below this, the clock has not been set by NTP yet. Matches the threshold
// main.cpp waits on during its time sync.
constexpr int64_t kClockSetEpoch = 1700000000;

// Local hour 0-23, or -1 when the clock has not been set.
int localHourFor(int64_t epochSeconds, int32_t utcOffsetSeconds);

// True inside the night window. An unset clock reads as day: a device still
// waiting on NTP should not sit in a dark palette for no reason, and it will
// correct itself within seconds of syncing.
bool isNightAt(int64_t epochSeconds, int32_t utcOffsetSeconds);
