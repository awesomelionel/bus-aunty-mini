// firmware/src/core/night_window.cpp
#include "core/night_window.h"

namespace {

constexpr int64_t kSecondsPerDay = 86400;
constexpr int64_t kSecondsPerHour = 3600;

}  // namespace

int localHourFor(int64_t epochSeconds, int32_t utcOffsetSeconds) {
    if (epochSeconds < kClockSetEpoch) {
        return -1;
    }
    int64_t local = epochSeconds + utcOffsetSeconds;
    // Defensive rather than necessary: no real timestamp plus a real offset
    // lands before the epoch, but a wrong-way offset should not index
    // backwards out of the day.
    int64_t intoDay = local % kSecondsPerDay;
    if (intoDay < 0) {
        intoDay += kSecondsPerDay;
    }
    return static_cast<int>(intoDay / kSecondsPerHour);
}

bool isNightAt(int64_t epochSeconds, int32_t utcOffsetSeconds) {
    const int hour = localHourFor(epochSeconds, utcOffsetSeconds);
    if (hour < 0) {
        return false;
    }
    // The window wraps midnight, so it is two ranges joined rather than one
    // comparison: 19:00-23:59 and 00:00-06:59.
    return hour >= kNightStartHour || hour < kNightEndHour;
}
