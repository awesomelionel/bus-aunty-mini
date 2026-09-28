#pragma once
#include <cstddef>
#include <cstdint>

#include "core/backoff_scheduler.h"
#include "core/night_window.h"

// What the arrivals loop should put on the panel. Pure so the firmware and
// the host tests share one decision.
enum class ArrivalScreen {
    Loading,       // clock not set yet
    NotFound,      // HTTP 404: "No data / Check stop code"
    NoRecentData,  // cache older than 10 minutes
    NoServices,    // the stop answered, and it had no buses
    NoMoreBuses,   // it had buses, and every one is now too far past its ETA
    Arrivals,
};

enum class FetchClass {
    Ok,
    NotFound,  // 404: do not parse, do not back off, do not count as an error
    Backoff,   // keep the last good data and lengthen the retry
};

// Status is classified before any JSON parse. 404 wins even when the body
// was not looked at. Anything that is not a 200 with a matched stop backs off.
inline FetchClass classifyFetch(int httpStatus, bool parsedStop) {
    if (httpStatus == 404) {
        return FetchClass::NotFound;
    }
    if (httpStatus != 200 || !parsedStop) {
        return FetchClass::Backoff;
    }
    return FetchClass::Ok;
}

// `hadArrivals` is true when the last good payload contained at least one
// timed bus, including buses later dropped for being too far in the past.
inline ArrivalScreen selectArrivalScreen(bool clockSet, bool notFound,
                                         bool hadArrivals, bool rowsRemain,
                                         uint32_t ageMs) {
    if (!clockSet) {
        return ArrivalScreen::Loading;
    }
    if (notFound) {
        return ArrivalScreen::NotFound;
    }
    if (ageMs >= kStaleDataThresholdMs) {
        return ArrivalScreen::NoRecentData;
    }
    if (!hadArrivals) {
        return ArrivalScreen::NoServices;
    }
    if (!rowsRemain) {
        return ArrivalScreen::NoMoreBuses;
    }
    return ArrivalScreen::Arrivals;
}

// Ok and 404 both return to the healthy 60s interval. Only a failed fetch
// advances 120s, then 240s, then 300s. A Wi-Fi drop is not a fetch outcome,
// so it must not call this.
inline void applyFetchOutcome(BackoffState& state, FetchClass outcome) {
    if (outcome == FetchClass::Backoff) {
        incrementBackoff(state);
        return;
    }
    resetBackoff(state);
}

// The worse of "when we last stored a payload" and "how old UpdatedAt is".
// A backend that keeps returning 200 with a frozen UpdatedAt still goes stale.
// Without a set clock only the fetch age is known.
inline uint32_t arrivalDataAgeMs(uint32_t fetchAgeMs, int64_t nowEpoch,
                                 int64_t updatedAtEpoch) {
    uint32_t age = fetchAgeMs;
    if (nowEpoch < kClockSetEpoch || updatedAtEpoch < 0 ||
        nowEpoch < updatedAtEpoch) {
        return age;
    }
    const int64_t seconds = nowEpoch - updatedAtEpoch;
    if (seconds >= static_cast<int64_t>(0xFFFFFFFF) / 1000) {
        return 0xFFFFFFFF;
    }
    const uint32_t updatedAge = static_cast<uint32_t>(seconds) * 1000;
    return updatedAge > age ? updatedAge : age;
}

// NTP has not set the clock, and the payload carries a usable UpdatedAt.
inline bool shouldSeedClock(int64_t nowEpoch, int64_t updatedAtEpoch) {
    return nowEpoch < kClockSetEpoch && updatedAtEpoch >= kClockSetEpoch;
}

// Painting "Syncing time..." for 15s is only useful while the clock is unset.
// A reconnect with a live clock starts NTP without covering the cache.
inline bool shouldBlockForNtp(int64_t nowEpoch) {
    return nowEpoch < kClockSetEpoch;
}

// Redraw the cached arrivals on a state change (page, fetch, Wi-Fi flag) or
// on the normal display interval. A 50ms loop pass with neither is not a redraw.
inline bool shouldRedrawCached(bool stateChanged, uint32_t lastRefreshMs,
                               uint32_t nowMs) {
    return stateChanged || shouldRefreshDisplay(lastRefreshMs, nowMs);
}

// An index past the end of a shrunk stop list, or any index when the list is
// empty, goes back to the first stop.
inline size_t clampStopIndex(size_t index, size_t stopCount) {
    if (stopCount == 0 || index >= stopCount) {
        return 0;
    }
    return index;
}
