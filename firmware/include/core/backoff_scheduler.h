#pragma once
#include <cstdint>
#include <cstddef>

// Backoff intervals in milliseconds
constexpr uint32_t kBackoffIntervalsMs[] = {60000, 120000, 240000, 300000, 300000};
constexpr size_t kMaxBackoffStep = sizeof(kBackoffIntervalsMs) / sizeof(kBackoffIntervalsMs[0]);
constexpr uint32_t kStaleDataThresholdMs = 600000;  // 10 minutes
constexpr uint32_t kDisplayRefreshIntervalMs = 15000;  // 15 seconds

struct BackoffState {
    size_t step = 0;
    uint32_t lastAttemptMs = 0;
};

// Returns true if enough time has passed since last attempt given current backoff
bool shouldAttemptFetch(const BackoffState& state, uint32_t nowMs);

// Returns backoff interval in ms for current step
uint32_t backoffIntervalMs(size_t step);

// Increment backoff step (caps at max)
void incrementBackoff(BackoffState& state);

// Reset backoff on success
void resetBackoff(BackoffState& state);

// Returns true if data is older than 10 minutes (wrap-safe)
bool isDataStale(uint32_t dataTimestampMs, uint32_t nowMs);

// Returns age in ms (wrap-safe)
uint32_t dataAgeMs(uint32_t dataTimestampMs, uint32_t nowMs);

// Returns true if should refresh display (every 15-30s)
bool shouldRefreshDisplay(uint32_t lastRefreshMs, uint32_t nowMs);
