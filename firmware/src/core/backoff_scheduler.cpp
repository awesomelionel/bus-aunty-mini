#include "core/backoff_scheduler.h"

bool shouldAttemptFetch(const BackoffState& state, uint32_t nowMs) {
    if (state.lastAttemptMs == 0) {
        return true;  // First attempt
    }
    
    uint32_t interval = backoffIntervalMs(state.step);
    // Wrap-safe comparison: if nowMs - lastAttemptMs >= interval
    uint32_t elapsed = nowMs - state.lastAttemptMs;
    return elapsed >= interval;
}

uint32_t backoffIntervalMs(size_t step) {
    if (step >= kMaxBackoffStep) {
        return kBackoffIntervalsMs[kMaxBackoffStep - 1];
    }
    return kBackoffIntervalsMs[step];
}

void incrementBackoff(BackoffState& state) {
    if (state.step < kMaxBackoffStep - 1) {
        ++state.step;
    }
}

void resetBackoff(BackoffState& state) {
    state.step = 0;
}

bool isDataStale(uint32_t dataTimestampMs, uint32_t nowMs) {
    uint32_t age = nowMs - dataTimestampMs;
    return age >= kStaleDataThresholdMs;
}

uint32_t dataAgeMs(uint32_t dataTimestampMs, uint32_t nowMs) {
    return nowMs - dataTimestampMs;
}

bool shouldRefreshDisplay(uint32_t lastRefreshMs, uint32_t nowMs) {
    if (lastRefreshMs == 0) {
        return false;  // No refresh needed initially
    }
    uint32_t elapsed = nowMs - lastRefreshMs;
    return elapsed >= kDisplayRefreshIntervalMs;
}
