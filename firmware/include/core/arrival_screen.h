#pragma once
#include <cstdint>

#include "core/backoff_scheduler.h"

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

// A Wi-Fi drop keeps the cached arrivals on screen. With no cache there is
// nothing to keep, and the panel says "No WiFi".
inline bool keepArrivalsOnWifiLoss(bool cacheValid) { return cacheValid; }
